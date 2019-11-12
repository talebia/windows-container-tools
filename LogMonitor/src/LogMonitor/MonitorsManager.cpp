//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

using namespace std;

///
/// MonitorsManager.cpp
/// 
/// 
/// 
///
MonitorsManager::_StaticDestructor MonitorsManager::s_staticDestructor = {};
MonitorsManager* MonitorsManager::s_instance = nullptr;

MonitorsManager::MonitorsManager(
    _In_ const std::wstring& ConfigFileName
    )
{
    DWORD status = ERROR_SUCCESS;

    std::wstring longConfigFilePath = Utility::GetLongPath(ConfigFileName);

    //
    // Get the directory path, in long format.
    //
    m_longDirectoryPath = longConfigFilePath;
    PathRemoveFileSpecW(&m_longDirectoryPath[0]);
    m_longDirectoryPath.resize(wcslen(m_longDirectoryPath.c_str()));

    //
    // Get the filename of the config file, without the directory part.
    //
    m_longConfigFileName = longConfigFilePath.substr(m_longDirectoryPath.size() + 1);

    //
    // Check that the config file's directory exists.
    //
    m_dirHandle = CreateFileW(m_longDirectoryPath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (m_dirHandle == INVALID_HANDLE_VALUE)
    {
        status = GetLastError();
    }

    if (status == ERROR_FILE_NOT_FOUND ||
        status == ERROR_PATH_NOT_FOUND)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Config file directory '%s' wasn't found.", m_longDirectoryPath.c_str()).c_str()
        );

        throw std::system_error(std::error_code(status, std::system_category()), "CreateFileW");
    }

    m_overlappedEvent = CreateEvent(nullptr, TRUE, TRUE, nullptr);
    if (!m_overlappedEvent)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateEvent");
    }

    m_overlapped.hEvent = m_overlappedEvent;

    status = SetDirectoryChangesListener();
    
    if (status != ERROR_SUCCESS)
    {
        throw std::system_error(std::error_code(status, std::system_category()), "SetDirectoryChangesListener");
    }

    std::wstring shortConfigFilePath = Utility::GetShortPath(ConfigFileName);

    //
    // Get the directory path, in short format.
    //
    std::wstring shortDirectoryPath = shortConfigFilePath;
    PathRemoveFileSpecW(&shortDirectoryPath[0]);
    shortDirectoryPath.resize(wcslen(shortDirectoryPath.c_str()));

    //
    // Get the filename of the config file, without the directory part.
    //
    m_shortConfigFileName = shortConfigFilePath.substr(shortDirectoryPath.size() + 1);
}

MonitorsManager::~MonitorsManager()
{
    if (!m_overlappedEvent)
    {
        CloseHandle(m_overlappedEvent);
    }

    if (m_dirHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_dirHandle);
    }
}

MonitorsManager::_StaticDestructor::~_StaticDestructor()
{
    if (MonitorsManager::s_instance != nullptr)
    {
        delete MonitorsManager::s_instance;
    }
}

DWORD 
MonitorsManager::Initialize(
    _In_ const std::wstring& ConfigFileName
)
{
    if (s_instance == nullptr)
    {
        try
        {
            s_instance = new MonitorsManager(ConfigFileName);
        }
        catch (std::exception & ex)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to create MonitorsManager. %S", ex.what()).c_str()
            );

            return E_FAIL;
        }
        catch(...)
        {
            return E_FAIL;
        }
    }

    return ERROR_SUCCESS;
}

MonitorsManager*
MonitorsManager::GetInstance()
{
    return s_instance;
}

DWORD
MonitorsManager::SetDirectoryChangesListener()
{
    DWORD status = ERROR_SUCCESS;

    std::fill(records.begin(), records.end(), (BYTE)0); // Reset previous entries if any.

    m_overlapped.Offset = 0;
    m_overlapped.OffsetHigh = 0;
    
    BOOL success = ReadDirectoryChangesW(m_dirHandle,
                                         records.data(),
                                         static_cast<ULONG>(records.size()),
                                         true,
                                         (FILE_NOTIFY_CHANGE_CREATION |
                                             FILE_NOTIFY_CHANGE_DIR_NAME |
                                             FILE_NOTIFY_CHANGE_FILE_NAME |
                                             FILE_NOTIFY_CHANGE_LAST_WRITE |
                                             FILE_NOTIFY_CHANGE_SIZE),
                                         nullptr,
                                         &m_overlapped,
                                         nullptr);

    if (!success)
    {
        status = GetLastError();
        if (status == ERROR_NOTIFY_ENUM_DIR)
        {
            status = ERROR_SUCCESS;
        }
    }

    return status;
}

bool
MonitorsManager::ConfigFileChanged()
{
    DWORD status = ERROR_SUCCESS;
    DWORD dwBytesTransfered = 0;
    DWORD dwNextEntryOffset = 0;

    bool configFileChanged = false;

    if (!GetOverlappedResult(m_dirHandle, &m_overlapped, &dwBytesTransfered, FALSE))
    {
        status = GetLastError();

        logWriter.TraceError(
            Utility::FormatString(L"Failed to retrieve the result of the overlapped operation. Error: %lu", status).c_str()
        );


        return false;
    }

    if (dwBytesTransfered)
    {
        FILE_NOTIFY_INFORMATION* fileNotificationInfo = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(records.data());
        WCHAR pszFileName[4096];
        do
        {
            //
            // Extract the FileName
            //
            DWORD dwFileNameLength = fileNotificationInfo->FileNameLength / sizeof(WCHAR);
            if (dwFileNameLength > 4096)
            {
                dwFileNameLength = 4095;
            }
            CopyMemory(pszFileName, fileNotificationInfo->FileName, sizeof(WCHAR) * dwFileNameLength);
            pszFileName[dwFileNameLength] = '\0';

            //
            // Get the long-format relative path of the event's filename.
            //
            std::wstring fileName = pszFileName;

            if (fileNotificationInfo->Action == FILE_ACTION_ADDED ||
                fileNotificationInfo->Action == FILE_ACTION_RENAMED_NEW_NAME)
            {
                std::wstring fullPath = Utility::GetLongPath(m_longDirectoryPath + L"\\" + m_longConfigFileName);

                //
                // Get the filename of the config file, without the directory part.
                //
                fileName = fullPath.substr(m_longDirectoryPath.size() + 1);
            }

            if (fileName == m_longConfigFileName || fileName == m_shortConfigFileName)
            {
                configFileChanged = true;
                
                //
                // If the file was added, get its short name, because it could have change.
                //
                if (fileNotificationInfo->Action == FILE_ACTION_ADDED ||
                    fileNotificationInfo->Action == FILE_ACTION_RENAMED_NEW_NAME)
                {
                    std::wstring shortConfigFilePath = Utility::GetShortPath(m_longDirectoryPath + L"\\" + m_longConfigFileName);

                    //
                    // Get the directory path, in short format.
                    //
                    std::wstring shortDirectoryPath = shortConfigFilePath;
                    PathRemoveFileSpecW(&shortDirectoryPath[0]);
                    shortDirectoryPath.resize(wcslen(shortDirectoryPath.c_str()));

                    //
                    // Get the filename of the config file, without the directory part.
                    //
                    m_shortConfigFileName = shortConfigFilePath.substr(shortDirectoryPath.size() + 1);
                }
            }

        } while (dwNextEntryOffset);
    }

    status = SetDirectoryChangesListener();

    return configFileChanged;
}

HANDLE
MonitorsManager::GetOverlappedEvent()
{
    return m_overlappedEvent;
}

DWORD
MonitorsManager::ReloadConfigFile()
{
    std::cout << "RELOADED" << std::endl;
    return ERROR_SUCCESS;
}