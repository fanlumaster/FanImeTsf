#pragma once

#include "Globals.h"

//////////////////////////////////////////////////////////////////////
//

class CParserStringRange : public CStringRange
{
  public:
    CParserStringRange() : CStringRange()
    {
        _fEscapeIncluded = FALSE;
    }

    BOOL _fEscapeIncluded; // flag. This string range included escape sequence.
};

//////////////////////////////////////////////////////////////////////
//

class CDictionaryParser
{
  public:
    CDictionaryParser(LCID locale);
    virtual ~CDictionaryParser();

    BOOL ParseLine(_In_reads_(dwBufLen) LPCWSTR pwszBuffer, DWORD_PTR dwBufLen, _Out_ CParserStringRange *psrgKeyword,
                   _Inout_opt_ CMetasequoiaImeArray<CParserStringRange> *pValue = nullptr);

    // dwBufLen - in character count
    _Ret_maybenull_ LPCWSTR GetToken(_In_reads_(dwBufLen) LPCWSTR pwszBuffer, DWORD_PTR dwBufLen,
                                     _In_ const WCHAR chDelimiter, _Out_ CParserStringRange *srgKeyWord);

  protected:
    BOOL RemoveWhiteSpaceFromBegin(_Inout_opt_ CStringRange *pString);
    BOOL RemoveWhiteSpaceFromEnd(_Inout_opt_ CStringRange *pString);
    BOOL RemoveStringDelimiter(_Inout_opt_ CStringRange *pString);

    DWORD_PTR GetOneLine(_In_z_ LPCWSTR pwszBuffer, DWORD_PTR dwBufLen);

    LCID _locale; // used for CompareString
};
