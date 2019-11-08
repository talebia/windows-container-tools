//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "Version.h"

using namespace std;

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "ws2_32.lib")  // For ntohs function
#pragma comment(lib, "shlwapi.lib") 

#define ARGV_OPTION_CONFIG_FILE L"/Config"
#define ARGV_OPTION_HELP1 L"/?"
#define ARGV_OPTION_HELP2 L"--help"



LogWriter logWriter;

HANDLE g_hStopEvent = INVALID_HANDLE_VALUE;

std::shared_ptr<LoggerSettings> currentSettings;

std::unique_ptr<EventMonitor> g_eventMon(nullptr);
std::vector<std::shared_ptr<LogFileMonitor>> g_logfileMonitors;
std::unique_ptr<EtwMonitor> g_etwMon(nullptr);

void ControlHandle(_In_ DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        {
            wprintf(L"\nCTRL signal received. The process will now terminate.\n");
            SetEvent(g_hStopEvent);
            g_hStopEvent = INVALID_HANDLE_VALUE;
            break;
        }

        default:
            return;
    }
}

void PrintUsage()
{
    wprintf(L"\n\tLogMonitor Tool Version %d.%d.%d.%d \n\n", LM_MAJORNUMBER, LM_MINORNUMBER, LM_BUILDNUMBER, LM_BUILDMINORVERSION);
    wprintf(L"\tUsage: LogMonitor.exe [/?] | [--help] | [[/CONFIG <PATH>][COMMAND [PARAMETERS]]] \n\n");
    wprintf(L"\t/?|--help   Shows help information\n");
    wprintf(L"\t<PATH>      Specifies the path of the Json configuration file. This is\n");
    wprintf(L"\t            an optional parameter. If not specified, then default Json\n");
    wprintf(L"\t            configuration file path %ws is used\n", DEFAULT_CONFIG_FILENAME);
    wprintf(L"\tCOMMAND     Specifies the name of the executable to be run \n");
    wprintf(L"\tPARAMETERS  Specifies the parameters to be passed to the COMMAND \n\n");
    wprintf(L"\tThis tool monitors Event log, ETW providers and log files and write the log entries\n");
    wprintf(L"\tto the console. The configuration of input log sources is specified in a Json file.\n");
    wprintf(L"\tfile.\n\n");
}

std::shared_ptr<LoggerSettings> 
GetChangedSettings(
    _In_ std::shared_ptr<LoggerSettings> CurrentSettings,
    _In_ std::shared_ptr<LoggerSettings> NewSettings
    )
{
    if (CurrentSettings == nullptr)
    {
        return NewSettings;
    }

}

bool StartMonitors(_In_ const PWCHAR ConfigFileName)
{
    bool success;

    std::wifstream configFileStream(ConfigFileName);
    if (!configFileStream.is_open())
    {
        logWriter.TraceError(
            Utility::FormatString(L"Configuration file '%s' not found. Logs will not be monitored.", ConfigFileName
            ).c_str()
        );

        return false;
    }

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
    catch (std::exception& ex)
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

    if (!success)
    {
        logWriter.TraceError(L"Invalid configuration file.");
        return success;
    }

    if (settings->Sources.EventLog != nullptr && !settings->Sources.EventLog->Channels.empty())
    {
        try
        {
            bool eventFormatMultiLine = settings->Sources.EventLog->EventFormatMultiLine != nullptr ?
                *settings->Sources.EventLog->EventFormatMultiLine :
                EVENT_MONITOR_MULTILINE_DEFAULT;

            bool eventMonStartAtOldestRecord = settings->Sources.EventLog->StartAtOldestRecord != nullptr ?
                *settings->Sources.EventLog->StartAtOldestRecord :
                EVENT_MONITOR_START_AT_OLDEST_RECORD_DEFAULT;

            g_eventMon = make_unique<EventMonitor>(settings->Sources.EventLog->Channels,
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

    for (auto logFileSource : settings->Sources.LogFiles)
    {
        try
        {
            std::shared_ptr<LogFileMonitor> logfileMon = make_shared<LogFileMonitor>(logFileSource->Directory, logFileSource->Filter, logFileSource->IncludeSubdirectories);
            g_logfileMonitors.push_back(std::move(logfileMon));
        }
        catch (std::exception& ex)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Instantiation of a LogFileMonitor object failed for directory %ws. %S", logFileSource->Directory.c_str(), ex.what()).c_str()
            );
        }
        catch (...)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Instantiation of a LogFileMonitor object failed for directory %ws. Unknown error occurred.", logFileSource->Directory.c_str()).c_str()
            );
        }
    }

    if (settings->Sources.ETW != nullptr && !settings->Sources.ETW->Providers.empty())
    {
        try
        {
            bool eventFormatMultiLine = settings->Sources.ETW->EventFormatMultiLine != nullptr ?
                *settings->Sources.ETW->EventFormatMultiLine :
                ETW_MONITOR_MULTILINE_DEFAULT;

            g_etwMon = make_unique<EtwMonitor>(settings->Sources.ETW->Providers,
                eventFormatMultiLine);
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

    return success;
}

void StopMonitors()
{
    g_eventMon.reset(nullptr);
    g_logfileMonitors.clear();
    g_etwMon.reset(nullptr);
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    std::wstring cmdline;
    PWCHAR configFileName = (PWCHAR)DEFAULT_CONFIG_FILENAME;

    g_hStopEvent = CreateEvent(nullptr,            // default security attributes
                               TRUE,               // manual-reset event
                               FALSE,              // initial state is nonsignaled
                               nullptr);           // object name

    if (g_hStopEvent == NULL)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to create event. Error: %d", GetLastError()).c_str()
        );
        return 0;
    }

    //
    // Check if the option /Config was passed.
    //
    int indexCommandArgument = 1;

    if (argc == 2)
    {
        if ((_wcsnicmp(argv[1], ARGV_OPTION_HELP1, _countof(ARGV_OPTION_HELP1)) == 0) ||
            (_wcsnicmp(argv[1], ARGV_OPTION_HELP2, _countof(ARGV_OPTION_HELP2)) == 0))
        {
            PrintUsage();
            return 0;
        }
    }
    else if (argc >= 3)
    {
        if (_wcsnicmp(argv[1], ARGV_OPTION_CONFIG_FILE, _countof(ARGV_OPTION_CONFIG_FILE)) == 0)
        {
            configFileName = argv[2];
            indexCommandArgument = 3;
        }

    }

    StartMonitors(configFileName);

    //
    // Create the child process. 
    //

    if (argc > indexCommandArgument)
    {
        cmdline = argv[indexCommandArgument];

        for (int i = indexCommandArgument + 1; i < argc; i++)
        {
            cmdline += L" ";
            cmdline += argv[i];
        }

        CreateAndMonitorProcess(cmdline);
    }
    else
    {
        DWORD waitResult = WaitForSingleObjectEx(g_hStopEvent, INFINITE, TRUE);

        switch (waitResult)
        {
            case WAIT_OBJECT_0:
            case WAIT_IO_COMPLETION:
                break;

            default:
                logWriter.TraceError(
                    Utility::FormatString(L"Log monitor wait failed. Error: %d", GetLastError()).c_str()
                );
                break;
        }
    }

    if (g_hStopEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hStopEvent);
        g_hStopEvent = INVALID_HANDLE_VALUE;
    }

    return 0;
}
