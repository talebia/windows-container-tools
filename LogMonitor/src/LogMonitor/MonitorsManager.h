//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

class MonitorsManager final
{

public:
    MonitorsManager() = delete;

    ~MonitorsManager();

    static DWORD Initialize(
        _In_ const std::wstring& ConfigFileName
        );

    static MonitorsManager* GetInstance();

    bool ReloadConfigFile();

    HANDLE GetOverlappedEvent();

    bool ConfigFileChanged();

    class _StaticDestructor
    {
    public:
        ~_StaticDestructor();
    };

private:
    MonitorsManager(
        _In_ const std::wstring& ConfigFileName
        );

    DWORD SetDirectoryChangesListener();

    void ApplyChangesToEventMonitor(
        _In_ std::shared_ptr<LoggerSettings> NewSettings
        );

    void ApplyChangesToLogFileMonitors(
        _In_ std::shared_ptr<LoggerSettings> NewSettings
        );

    void ApplyChangesToEtwMonitor(
        _In_ std::shared_ptr<LoggerSettings> NewSettings
        );

    //
    // Singleton variables
    //
    static MonitorsManager* s_instance;
    static _StaticDestructor s_staticDestructor;

    //
    // Members used for file changes listening.
    //
    std::wstring m_longDirectoryPath;
    std::wstring m_longConfigFileName;
    std::wstring m_shortConfigFileName;

    HANDLE m_dirHandle;

    OVERLAPPED m_overlapped;
    HANDLE m_overlappedEvent;

    UINT64 m_lastReadTimestamp = 0;

    //
    // Must be DWORD aligned so allocated on the heap.
    //
    std::vector<BYTE> records = std::vector<BYTE>(8192);

    //
    // Monitors
    //
    std::shared_ptr<LoggerSettings> m_currentSettings;

    std::vector<UINT> fileMonitorsIndexes;

    std::unique_ptr<EventMonitor> m_eventMon = nullptr;
    std::vector<std::shared_ptr<LogFileMonitor>> m_logFileMonitors;
    std::unique_ptr<EtwMonitor> m_etwMon = nullptr;
};