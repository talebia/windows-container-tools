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

bool
MonitorsManager::ReloadConfigFile()
{
    bool success;

    std::wifstream configFileStream(m_longDirectoryPath + L"\\" + m_longConfigFileName);
    if (!configFileStream.is_open())
    {
        logWriter.TraceError(
            Utility::FormatString(
                L"Configuration file '%s' not found. Logs will not be monitored.", 
                (m_longDirectoryPath + L"\\" + m_longConfigFileName).c_str()
            ).c_str()
        );

        success = false;
    }
    else
    {
        std::shared_ptr<LoggerSettings> settings = make_shared<LoggerSettings>();

        try
        {
            //
            // Convert the document content to a string, to pass it to JsonFileParser constructor.
            //
            std::wstring configFileStr((std::istreambuf_iterator<wchar_t>(configFileStream)),
                std::istreambuf_iterator<wchar_t>());

            JsonFileParser jsonParser(configFileStr);

            success = ReadConfigFile(jsonParser, *settings);
        }
        catch (std::exception & ex)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to read json configuration file. %S", ex.what()).c_str()
            );
            success = false;
        }
        catch (...)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to read json configuration file. Unknown error occurred.").c_str()
            );
            success = false;
        }

        if (success)
        {
            //
            // Apply the changes to the monitors.
            //
            ApplyChangesToEventMonitor(settings);
            ApplyChangesToLogFileMonitors(settings);
            ApplyChangesToEtwMonitor(settings);

            m_currentSettings = settings;
        }
        else
        {
            logWriter.TraceError(L"Invalid configuration file. Logs will not be monitored.");
        }
    }

    return success;
}

void 
MonitorsManager::ApplyChangesToEventMonitor(
    _In_ std::shared_ptr<LoggerSettings> NewSettings
    )
{
    bool stopMonitor = false;
    bool startMonitor = false;

    //
    // 
    //
    if (m_eventMon == nullptr && NewSettings->Sources.EventLog != nullptr)
    {
        startMonitor = true;
    }
    else if (m_eventMon != nullptr && NewSettings->Sources.EventLog == nullptr)
    {
        stopMonitor = true;
    }
    else if (m_eventMon != nullptr && NewSettings->Sources.EventLog != nullptr)
    {
        std::shared_ptr<SourceEventLog> oldEventMonitor = m_currentSettings->Sources.EventLog;

        bool oldEventFormatMultiLine = GET_VALUE_OR_DEFAULT(
            oldEventMonitor->EventFormatMultiLine,
            EVENT_MONITOR_MULTILINE_DEFAULT
        );

        bool oldEventMonStartAtOldestRecord = GET_VALUE_OR_DEFAULT(
            oldEventMonitor->StartAtOldestRecord,
            EVENT_MONITOR_START_AT_OLDEST_RECORD_DEFAULT
        );

        bool newEventFormatMultiLine = GET_VALUE_OR_DEFAULT(
            NewSettings->Sources.EventLog->EventFormatMultiLine,
            EVENT_MONITOR_MULTILINE_DEFAULT
        );

        bool newEventMonStartAtOldestRecord = GET_VALUE_OR_DEFAULT(
            NewSettings->Sources.EventLog->StartAtOldestRecord, 
            EVENT_MONITOR_START_AT_OLDEST_RECORD_DEFAULT
        );

        if (oldEventFormatMultiLine != newEventFormatMultiLine ||
            oldEventMonStartAtOldestRecord != newEventMonStartAtOldestRecord)
        {
            stopMonitor = true;
            startMonitor = true;
        }
        else
        {
            std::set<EventLogChannel> oldChannels(
                oldEventMonitor->Channels.begin(),
                oldEventMonitor->Channels.end()
            );

            std::set<EventLogChannel> newChannels(
                NewSettings->Sources.EventLog->Channels.begin(),
                NewSettings->Sources.EventLog->Channels.end()
            );

            if (oldChannels == newChannels)
            {
                stopMonitor = true;
                startMonitor = true;
            }
        }
    }


    if (stopMonitor)
    {
        m_eventMon.reset();
    }

    if (startMonitor)
    {
        try
        {
            bool eventFormatMultiLine = GET_VALUE_OR_DEFAULT(
                NewSettings->Sources.EventLog->EventFormatMultiLine,
                EVENT_MONITOR_MULTILINE_DEFAULT
            );

            bool eventMonStartAtOldestRecord = GET_VALUE_OR_DEFAULT(
                NewSettings->Sources.EventLog->StartAtOldestRecord,
                EVENT_MONITOR_START_AT_OLDEST_RECORD_DEFAULT
            );

            m_eventMon = make_unique<EventMonitor>(NewSettings->Sources.EventLog->Channels,
                eventFormatMultiLine,
                eventMonStartAtOldestRecord);
        }
        catch (std::exception & ex)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Instantiation of a EventMonitor object failed. %S", ex.what()).c_str()
            );
        }
        catch (...)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Instantiation of a EventMonitor object failed. Unknown error occurred.").c_str()
            );
        }
    }
}

void 
MonitorsManager::ApplyChangesToLogFileMonitors(
    _In_ std::shared_ptr<LoggerSettings> NewSettings
    )
{
    std::vector<UINT> newFileMonitorsIndexes;
    std::vector<std::shared_ptr<LogFileMonitor>> newLogFileMonitors;

    auto comp = [](std::pair<SourceFile, int> p1, std::pair<SourceFile, int> p2) {
        return p1.first < p2.first; 
    };
    auto newLogFileSources = std::set<std::pair<SourceFile, int>, decltype(comp)>(comp);

    for (int i = 0; i < NewSettings->Sources.LogFiles.size(); i++)
    {
        newLogFileSources.insert({ *NewSettings->Sources.LogFiles[i], i });
    }

    //
    // Copy the file monitors that haven't changed.
    //
    for (int i = 0; i < fileMonitorsIndexes.size(); i++)
    {
        bool stopMonitor = false;
        bool startMonitor = false;

        std::shared_ptr<SourceFile> oldSourceFileMonitor = m_currentSettings->Sources.LogFiles[fileMonitorsIndexes[i]];
        std::shared_ptr<LogFileMonitor> oldFileMonitor = m_logFileMonitors[i];

        auto sameSourceFile = newLogFileSources.find({ *oldSourceFileMonitor, 0 });

        if (sameSourceFile == newLogFileSources.end())
        {
            newLogFileMonitors.push_back(oldFileMonitor);
            newFileMonitorsIndexes.push_back(sameSourceFile->second);

            newLogFileSources.erase(sameSourceFile);
        }
    }

    //
    // Create the added file monitors.
    //
    for (auto pairSourceFile : newLogFileSources)
    {
        const SourceFile& sourceFile = pairSourceFile.first;
        const UINT originalIndex = pairSourceFile.second;

        try
        {
            std::shared_ptr<LogFileMonitor> logfileMon = make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
            newLogFileMonitors.push_back(std::move(logfileMon));
            newFileMonitorsIndexes.push_back(originalIndex);
        }
        catch (std::exception & ex)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Instantiation of a LogFileMonitor object failed for directory %ws. %S", sourceFile.Directory.c_str(), ex.what()).c_str()
            );
        }
        catch (...)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Instantiation of a LogFileMonitor object failed for directory %ws. Unknown error occurred.", sourceFile.Directory.c_str()).c_str()
            );
        }
    }

    //
    // Swa[ the new monitors array with the old one. The old file monitors that
    // must be deleted are going to be deleted when newLogFileMonitors is out 
    // of scope.
    //
    m_logFileMonitors.swap(newLogFileMonitors);
    fileMonitorsIndexes.swap(newFileMonitorsIndexes);
}


void
MonitorsManager::ApplyChangesToEtwMonitor(
    _In_ std::shared_ptr<LoggerSettings> NewSettings
)
{
    bool stopMonitor = false;
    bool startMonitor = false;

    //
    // 
    //
    if (m_etwMon == nullptr && NewSettings->Sources.ETW != nullptr)
    {
        startMonitor = true;
    }
    else if (m_etwMon != nullptr && NewSettings->Sources.ETW == nullptr)
    {
        stopMonitor = true;
    }
    else if (m_etwMon != nullptr && NewSettings->Sources.ETW != nullptr)
    {
        std::shared_ptr<SourceETW> oldEventMonitor = m_currentSettings->Sources.ETW;

        bool oldEventFormatMultiLine = GET_VALUE_OR_DEFAULT(
            oldEventMonitor->EventFormatMultiLine,
            ETW_MONITOR_MULTILINE_DEFAULT
        );

        bool newEventFormatMultiLine = GET_VALUE_OR_DEFAULT(
            NewSettings->Sources.EventLog->EventFormatMultiLine,
            ETW_MONITOR_MULTILINE_DEFAULT
        );


        if (oldEventFormatMultiLine != newEventFormatMultiLine )
        {
            stopMonitor = true;
            startMonitor = true;
        }
        else
        {
            std::set<ETWProvider> oldProviders(
                oldEventMonitor->Providers.begin(),
                oldEventMonitor->Providers.end()
            );

            std::set<ETWProvider> newProviders(
                NewSettings->Sources.ETW->Providers.begin(),
                NewSettings->Sources.ETW->Providers.end()
            );

            if (oldProviders == newProviders)
            {
                stopMonitor = true;
                startMonitor = true;
            }
        }
    }


    if (stopMonitor)
    {
        m_eventMon.reset();
    }

    if (startMonitor)
    {
        try
        {
            bool eventFormatMultiLine = GET_VALUE_OR_DEFAULT(
                NewSettings->Sources.EventLog->EventFormatMultiLine,
                ETW_MONITOR_MULTILINE_DEFAULT
            );

            m_etwMon = make_unique<EtwMonitor>(NewSettings->Sources.ETW->Providers,
                eventFormatMultiLine);
        }
        catch (std::exception & ex)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Instantiation of a EtwMonitor object failed. %S", ex.what()).c_str()
            );
        }
        catch (...)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Instantiation of a EtwMonitor object failed. Unknown error occurred.").c_str()
            );
        }
    }
}