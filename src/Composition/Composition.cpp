#include "Private.h"
#include "Globals.h"
#include "MetasequoiaIME.h"
#include "CompositionProcessorEngine.h"
#include <debugapi.h>
#include <fmt/xchar.h>
#include <string>
#include "FanyDefines.h"

//+---------------------------------------------------------------------------
//
// ITfCompositionSink::OnCompositionTerminated
//
// Callback for ITfCompositionSink.  The system calls this method whenever
// someone other than this service ends a composition.
//----------------------------------------------------------------------------

STDAPI CMetasequoiaIME::OnCompositionTerminated(TfEditCookie ecWrite, _In_ ITfComposition *pComposition)
{
    // Clear dummy composition
    _RemoveDummyCompositionForComposing(ecWrite, pComposition);

    // Clear display attribute and end composition, _EndComposition will release composition for us
    ITfContext *pContext = _pContext;
    if (pContext)
    {
        pContext->AddRef();
    }

    _EndComposition(_pContext);

    _DeleteCandidateList(FALSE, pContext);

    if (pContext)
    {
        pContext->Release();
        pContext = nullptr;
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _IsComposing
//
//----------------------------------------------------------------------------

BOOL CMetasequoiaIME::_IsComposing()
{
    return _pComposition != nullptr;
}

//+---------------------------------------------------------------------------
//
// _SetComposition
//
//----------------------------------------------------------------------------

void CMetasequoiaIME::_SetComposition(_In_ ITfComposition *pComposition)
{
    _pComposition = pComposition;
}

//+---------------------------------------------------------------------------
//
// _AddComposingAndChar
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_AddComposingAndChar(TfEditCookie ec, _In_ ITfContext *pContext,
                                              _In_ CStringRange *pstrAddString)
{
#ifdef FANY_DEBUG
    OutputDebugString(fmt::format(L"You entered _AddComposingAndChar{}\n", pstrAddString->Get()).c_str());
#endif
    HRESULT hr = S_OK;

    ULONG fetched = 0;
    TF_SELECTION tfSelection;

    if (pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched) != S_OK || fetched == 0)
        return S_FALSE;

    //
    // make range start to selection
    //
    ITfRange *pAheadSelection = nullptr;
    hr = pContext->GetStart(ec, &pAheadSelection);
    if (SUCCEEDED(hr))
    {
        hr = pAheadSelection->ShiftEndToRange(ec, tfSelection.range, TF_ANCHOR_START);
        if (SUCCEEDED(hr))
        {
            ITfRange *pRange = nullptr;
            // BOOL exist_composing = _FindComposingRange(ec, pContext, pAheadSelection, &pRange);

            _pComposition->GetRange(&pRange);

            std::wstring strAddString(pstrAddString->Get(), pstrAddString->GetLength());
            // _SetInputString(ec, pContext, pRange, pstrAddString, exist_composing);
            if (pRange)
            {
                pRange->SetText(ec, 0, strAddString.c_str(), (LONG)strAddString.length());

                /* Update the selection, we'll make it an insertion point just past */
                ITfRange *pSelection = nullptr;
                if (pRange && SUCCEEDED(pRange->Clone(&pSelection)))
                {
                    pSelection->Collapse(ec, TF_ANCHOR_END);

                    TF_SELECTION sel = {};
                    sel.range = pSelection;
                    sel.style.ase = TF_AE_NONE;
                    sel.style.fInterimChar = FALSE;

                    pContext->SetSelection(ec, 1, &sel);

                    pSelection->Release();
                }

                pRange->Release();
            }
        }
    }

    tfSelection.range->Release();

    if (pAheadSelection)
    {
        pAheadSelection->Release();
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _AddCharAndFinalize
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_AddCharAndFinalize(TfEditCookie ec, _In_ ITfContext *pContext,
                                             _In_ CStringRange *pstrAddString)
{
    HRESULT hr = E_FAIL;

    ULONG fetched = 0;
    TF_SELECTION tfSelection;

    if ((hr = pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched)) != S_OK || fetched != 1)
        return hr;

    // We use SetText here instead of InsertTextAtSelection because we've already started a composition
    // We don't want to the app to adjust the insertion point inside our composition
    hr = tfSelection.range->SetText(ec, 0, pstrAddString->Get(), (LONG)pstrAddString->GetLength());
    if (hr == S_OK)
    {
        // Update the selection, we'll make it an insertion point just past
        // the inserted text.
        tfSelection.range->Collapse(ec, TF_ANCHOR_END);
        pContext->SetSelection(ec, 1, &tfSelection);
    }

    tfSelection.range->Release();

    return hr;
}

//+---------------------------------------------------------------------------
//
// _FindComposingRange
//
//----------------------------------------------------------------------------

BOOL CMetasequoiaIME::_FindComposingRange(TfEditCookie ec, _In_ ITfContext *pContext, _In_ ITfRange *pSelection,
                                          _Outptr_result_maybenull_ ITfRange **ppRange)
{
    if (ppRange == nullptr)
    {
        return FALSE;
    }

    *ppRange = nullptr;

    // find GUID_PROP_COMPOSING
    ITfProperty *pPropComp = nullptr;
    IEnumTfRanges *enumComp = nullptr;

    HRESULT hr = pContext->GetProperty(GUID_PROP_COMPOSING, &pPropComp);
    if (FAILED(hr) || pPropComp == nullptr)
    {
        return FALSE;
    }

    hr = pPropComp->EnumRanges(ec, &enumComp, pSelection);
    if (FAILED(hr) || enumComp == nullptr)
    {
        pPropComp->Release();
        return FALSE;
    }

    BOOL isCompExist = FALSE;
    VARIANT var;
    ULONG fetched = 0;

    while (enumComp->Next(1, ppRange, &fetched) == S_OK && fetched == 1)
    {
        hr = pPropComp->GetValue(ec, *ppRange, &var);
        if (hr == S_OK)
        {
            if (var.vt == VT_I4 && var.lVal != 0)
            {
                isCompExist = TRUE;
                break;
            }
        }
        (*ppRange)->Release();
        *ppRange = nullptr;
    }

    pPropComp->Release();
    enumComp->Release();

    return isCompExist;
}

//+---------------------------------------------------------------------------
//
// _SetInputString
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_SetInputString(TfEditCookie ec, _In_ ITfContext *pContext, _Out_opt_ ITfRange *pRange,
                                         _In_ CStringRange *pstrAddString, BOOL exist_composing)
{
    ITfRange *pRangeInsert = nullptr;
    if (!exist_composing)
    {
        _InsertAtSelection(ec, pContext, pstrAddString, &pRangeInsert);
        if (pRangeInsert == nullptr)
        {
            return S_OK;
        }
        pRange = pRangeInsert;
    }
    if (pRange != nullptr)
    {
        pRange->SetText(ec, 0, pstrAddString->Get(), (LONG)pstrAddString->GetLength());
    }

    _SetCompositionLanguage(ec, pContext);

    _SetCompositionDisplayAttributes(ec, pContext, _gaDisplayAttributeInput);

    // update the selection, we'll make it an insertion point just past
    // the inserted text.
    ITfRange *pSelection = nullptr;
    TF_SELECTION sel;

    if ((pRange != nullptr) && (pRange->Clone(&pSelection) == S_OK))
    {
        pSelection->Collapse(ec, TF_ANCHOR_END);

        sel.range = pSelection;
        sel.style.ase = TF_AE_NONE;
        sel.style.fInterimChar = FALSE;
        pContext->SetSelection(ec, 1, &sel);
        pSelection->Release();
    }

    if (pRangeInsert)
    {
        pRangeInsert->Release();
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _InsertAtSelection
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_InsertAtSelection(TfEditCookie ec, _In_ ITfContext *pContext,
                                            _In_ CStringRange *pstrAddString, _Outptr_ ITfRange **ppCompRange)
{
    ITfRange *rangeInsert = nullptr;
    ITfInsertAtSelection *pias = nullptr;
    HRESULT hr = S_OK;

    if (ppCompRange == nullptr)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    *ppCompRange = nullptr;

    hr = pContext->QueryInterface(IID_ITfInsertAtSelection, (void **)&pias);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = pias->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, pstrAddString->Get(), (LONG)pstrAddString->GetLength(),
                                     &rangeInsert);

    if (FAILED(hr) || rangeInsert == nullptr)
    {
        rangeInsert = nullptr;
        pias->Release();
        goto Exit;
    }

    *ppCompRange = rangeInsert;
    pias->Release();
    hr = S_OK;

Exit:
    return hr;
}

//+---------------------------------------------------------------------------
//
// _RemoveDummyCompositionForComposing
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_RemoveDummyCompositionForComposing(TfEditCookie ec, _In_ ITfComposition *pComposition)
{
    HRESULT hr = S_OK;

    ITfRange *pRange = nullptr;

    if (pComposition)
    {
        hr = pComposition->GetRange(&pRange);
        if (SUCCEEDED(hr))
        {
            pRange->SetText(ec, 0, nullptr, 0);
            pRange->Release();
        }
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// _SetCompositionLanguage
//
//----------------------------------------------------------------------------

BOOL CMetasequoiaIME::_SetCompositionLanguage(TfEditCookie ec, _In_ ITfContext *pContext)
{
    HRESULT hr = S_OK;
    BOOL ret = TRUE;

    CCompositionProcessorEngine *pCompositionProcessorEngine = nullptr;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;

    LANGID langidProfile = 0;
    pCompositionProcessorEngine->GetLanguageProfile(&langidProfile);

    ITfRange *pRangeComposition = nullptr;
    ITfProperty *pLanguageProperty = nullptr;

    // we need a range and the context it lives in
    hr = _pComposition->GetRange(&pRangeComposition);
    if (FAILED(hr))
    {
        ret = FALSE;
        goto Exit;
    }

    // get our the language property
    hr = pContext->GetProperty(GUID_PROP_LANGID, &pLanguageProperty);
    if (FAILED(hr))
    {
        ret = FALSE;
        goto Exit;
    }

    VARIANT var;
    var.vt = VT_I4; // we're going to set DWORD
    var.lVal = langidProfile;

    hr = pLanguageProperty->SetValue(ec, pRangeComposition, &var);
    if (FAILED(hr))
    {
        ret = FALSE;
        goto Exit;
    }

    pLanguageProperty->Release();
    pRangeComposition->Release();

Exit:
    return ret;
}
