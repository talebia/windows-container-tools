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

    DWORD status = MonitorsManager::Initialize(configFileName);
    /// TODO: remove this if
    if (status != ERROR_SUCCESS)
    {
        return status;
    }

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
        bool bStop = false;
        HANDLE events[2] = { g_hStopEvent };
        events[1] = MonitorsManager::GetInstance()->GetOverlappedEvent();

        while (!bStop)
        {
            DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

            switch (waitResult)
            {
                case WAIT_OBJECT_0:
                case WAIT_IO_COMPLETION:
                    bStop = true;
                    break;

                case WAIT_OBJECT_0 + 1:
                    if (MonitorsManager::GetInstance()->ConfigFileChanged())
                    {
                        MonitorsManager::GetInstance()->ReloadConfigFile();
                    }
                    break;

                default:
                    logWriter.TraceError(
                        Utility::FormatString(L"Log monitor wait failed. Error: %d", GetLastError()).c_str()
                    );
                    bStop = true;
                    break;
            }
        }
    }

    if (g_hStopEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hStopEvent);
        g_hStopEvent = INVALID_HANDLE_VALUE;
    }

    return 0;
}
