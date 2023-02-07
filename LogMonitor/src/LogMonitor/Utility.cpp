//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

using namespace std;


///
/// Utility.cpp
///
/// Contains utility functions to format strings, convert time to string, etc.
///


///
/// Returns the string representation of a SYSTEMTIME structure that can be used
/// in an XML query for Windows Event collection.
///
/// \param SystemTime         SYSTEMTIME with the time to format.
///
/// \return The string with the human-readable time.
///
std::wstring
Utility::SystemTimeToString(
    SYSTEMTIME SystemTime
    )
{
    constexpr size_t STR_LEN = 64;
    wchar_t dateStr[STR_LEN] = { 0 };

#pragma warning(suppress : 38042)
    GetDateFormatEx(0, 0, &SystemTime, L"yyyy-MM-dd", dateStr, STR_LEN, 0);

    wchar_t timeStr[STR_LEN] = { 0 };
    GetTimeFormatEx(0, 0, &SystemTime, L"HH:mm:ss", timeStr, STR_LEN);

    return FormatString(L"%sT%s.000Z", dateStr, timeStr);
}


///
/// Returns a string representation of a FILETIME.
///
/// \param FileTime         FILETIME with the time to format.
///
/// \return The string with the human-readable time.
///
std::wstring
Utility::FileTimeToString(
    FILETIME FileTime
    )
{
    SYSTEMTIME systemTime;
    FileTimeToSystemTime(&FileTime, &systemTime);
    return SystemTimeToString(systemTime);
}

///
/// Returns a formatted wide string.
///
/// \param FormatString         Format string that follows the same specifications as format in printf.
/// \param ...                  Variable arguments list
///
/// \return The composed string.
///
std::wstring
Utility::FormatString(
    _In_ _Printf_format_string_ LPCWSTR FormatString,
    ...
    )
{
    std::wstring result = L"";
    va_list vaList;
    va_start(vaList, FormatString);

    size_t length = ::_vscwprintf(FormatString, vaList);

    try
    {
        result.resize(length);

        if (-1 == ::vswprintf_s(&result[0], length + 1, FormatString, vaList))
        {
            //
            // Ignore failure and continue
            //
        }
    }
    catch (...) {}

    va_end(vaList);

    return result;
}

///
/// Verify if the input stream is in UTF-8 format.
/// UTF-8 is the encoding of Unicode based on Internet Society RFC2279
/// ( See https://tools.ietf.org/html/rfc2279 )
///
/// \param lpstrInputStream     Pointer to a buffer with the wide text to evaluate.
/// \param iLen                 Size of the buffer.
///
/// \return If the text is UTF-8, return true. Otherwise, false.
///
bool
Utility::IsTextUTF8(
    LPCSTR InputStream,
    int Length
    )
{
    int nChars = MultiByteToWideChar(CP_UTF8,
                                      MB_ERR_INVALID_CHARS,
                                      InputStream,
                                      Length,
                                      NULL,
                                      0);

    return nChars > 0 || GetLastError() != ERROR_NO_UNICODE_TRANSLATION;
}

///
/// Verify if the input stream is in Unicode format.
///
/// \param lpstrInputStream     Pointer to a buffer with the wide text to evaluate.
/// \param iLen                 Size of the buffer.
///
/// \return If the text is unicode, return true. Otherwise, false.
///
bool
Utility::IsInputTextUnicode(
    LPCSTR InputStream,
    int Length
    )
{
    int  iResult = ~0; // turn on IS_TEXT_UNICODE_DBCS_LEADBYTE
    bool bUnicode;

    bUnicode = IsTextUnicode(InputStream, Length, &iResult);

    //
    // If the only hint is based on statistics, assume ANSI for short strings
    // This protects against short ansi strings like "this program can break" from being
    // detected as unicode
    //
    if (bUnicode && iResult == IS_TEXT_UNICODE_STATISTICS && Length < 100)
    {
        bUnicode = false;
    }

    return bUnicode;
}


///
/// Get the short path name of the file. If the function
/// failed to get the short path, then it returns original path.
///
/// \param Path Full path of a file or directory
///
/// \return Short path of a file or directory
///
std::wstring
Utility::GetShortPath(
    _In_ const std::wstring& Path
    )
{
    DWORD bufSz = 1024;
    std::vector<wchar_t> buf(bufSz);

    DWORD pathSz = GetShortPathNameW(Path.c_str(), &(buf[0]), bufSz);
    if (pathSz >= bufSz)
    {
        buf.resize(pathSz + 1);
        if (GetShortPathNameW(Path.c_str(), &(buf[0]), pathSz + 1))
        {
            wstring shortPath = &buf[0];
            return shortPath.c_str();
        }
    }
    else if (pathSz)
    {
        wstring shortPath = &buf[0];
        return shortPath.c_str();
    }

    return Path;
}


///
/// Get the long path name of the file. If the function
/// failed to get the long path, then it returns original path.
///
/// \param Path Full path of a file or directory
///
/// \return Long path of a file or directory
///
std::wstring
Utility::GetLongPath(
    _In_ const std::wstring& Path
    )
{
    DWORD bufSz = 1024;
    std::vector<wchar_t> buf(bufSz);

    DWORD pathSz = GetLongPathNameW(Path.c_str(), &(buf[0]), bufSz);
    if(pathSz >= bufSz)
    {
        buf.resize(pathSz + 1);
        if (GetLongPathNameW(Path.c_str(), &(buf[0]), pathSz + 1))
        {
            wstring longPath = &buf[0];
            return longPath.c_str();
        }
    }
    else if (pathSz)
    {
        wstring longPath = &buf[0];
        return longPath.c_str();
    }

    return Path;
}

///
/// Replaces all the occurrences in a wstring.
///
/// \param Str      The string to search substrings and replace them.
/// \param From     The substring to being replaced.
/// \param To       The substring to replace.
///
/// \return A wstring.
///
std::wstring Utility::ReplaceAll(_In_ std::wstring Str, _In_ const std::wstring& From, _In_ const std::wstring& To) {
    size_t start_pos = 0;

    while ((start_pos = Str.find(From, start_pos)) != std::string::npos) {
        Str.replace(start_pos, From.length(), To);
        start_pos += To.length(); // Handles case where 'To' is a substring of 'From'
    }
    return Str;
}

std::wstring Utility::STR_TO_W_STR(_In_ const std::string& str) {
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.from_bytes(str);
}

std::string Utility::W_STR_TO_STR(_In_ const std::wstring& wstr) {
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}

LONG Utility::GetDWORDRegKey(
    _In_ HKEY Key,
    _Inout_ const std::wstring& ValueName,
    _In_ DWORD& Value,
    _In_ DWORD DefaultValue
    )
{
    Value = DefaultValue;
    DWORD wBufferSize(sizeof(DWORD));
    DWORD Result(0);
    LONG Error = ::RegQueryValueExW(
        Key,
        ValueName.c_str(),
        0,
        NULL,
        reinterpret_cast<LPBYTE>(&Result),
        &wBufferSize);
        if (ERROR_SUCCESS == Error) {
            Value = Result;
    }
    return Error;
}

LONG Utility::GetBoolRegKey(
    _In_ HKEY Key,
    _Inout_ const std::wstring& ValueName,
    _In_ bool& Value,
    _In_ bool DefaultValue
    )
{
    DWORD DefValue((DefaultValue) ? 1 : 0);
    DWORD Result(DefValue);
    LONG Error = Utility::GetDWORDRegKey(Key, ValueName.c_str(), Result, DefValue);
    if (ERROR_SUCCESS == Error) {
        Value = (Result != 0) ? true : false;
    }
    return Error;
}

LONG Utility::GetStringRegKey(
    _In_ HKEY Key,
    _Inout_ const std::wstring& ValueName,
    _In_ std::wstring& Value,
    _In_ const std::wstring& DefaultValue
    )
{
    Value = DefaultValue;
    WCHAR Buffer[512];
    DWORD BufferSize = sizeof(Buffer);
    ULONG Error;
    Error = RegQueryValueExW(Key, ValueName.c_str(), 0, NULL, (LPBYTE)Buffer, &BufferSize);
    if (ERROR_SUCCESS == Error) {
        Value = Buffer;
    }
    return Error;
}

