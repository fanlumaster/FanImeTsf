// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Ipc.h"
#include "Private.h"
#include "SampleIME.h"
#include "CandidateListUIPresenter.h"
#include <debugapi.h>

//+---------------------------------------------------------------------------
//
// ITfTextLayoutSink::OnSetThreadFocus
//
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnSetThreadFocus()
{
    // Connect to namedpipe
    InitNamedpipe();
    if (_pCandidateListUIPresenter)
    {
        ITfDocumentMgr *pCandidateListDocumentMgr = nullptr;
        ITfContext *pTfContext = _pCandidateListUIPresenter->_GetContextDocument();

        if ((nullptr != pTfContext) && SUCCEEDED(pTfContext->GetDocumentMgr(&pCandidateListDocumentMgr)))
        {
            if (pCandidateListDocumentMgr == _pDocMgrLastFocused)
            {
                _pCandidateListUIPresenter->OnSetThreadFocus();
            }

            pCandidateListDocumentMgr->Release();
        }
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfTextLayoutSink::OnKillThreadFocus
//
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnKillThreadFocus()
{
    // Disconnect from namedpipe
    CloseNamedpipe();
    if (_pCandidateListUIPresenter)
    {
        ITfDocumentMgr *pCandidateListDocumentMgr = nullptr;
        ITfContext *pTfContext = _pCandidateListUIPresenter->_GetContextDocument();

        if ((nullptr != pTfContext) && SUCCEEDED(pTfContext->GetDocumentMgr(&pCandidateListDocumentMgr)))
        {
            if (_pDocMgrLastFocused)
            {
                _pDocMgrLastFocused->Release();
                _pDocMgrLastFocused = nullptr;
            }
            _pDocMgrLastFocused = pCandidateListDocumentMgr;
            if (_pDocMgrLastFocused)
            {
                _pDocMgrLastFocused->AddRef();
            }
        }
        _pCandidateListUIPresenter->OnKillThreadFocus();
    }
    return S_OK;
}

BOOL CSampleIME::_InitThreadFocusSink()
{
    ITfSource *pSource = nullptr;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        return FALSE;
    }

    if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink, (ITfThreadFocusSink *)this, &_dwThreadFocusSinkCookie)))
    {
        pSource->Release();
        return FALSE;
    }

    pSource->Release();

    return TRUE;
}

void CSampleIME::_UninitThreadFocusSink()
{
    ITfSource *pSource = nullptr;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        return;
    }

    if (FAILED(pSource->UnadviseSink(_dwThreadFocusSinkCookie)))
    {
        pSource->Release();
        return;
    }

    pSource->Release();
}
