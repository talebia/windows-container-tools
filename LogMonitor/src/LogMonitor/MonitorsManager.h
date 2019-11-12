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

    DWORD ReloadConfigFile();

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

    static MonitorsManager* s_instance;
    static _StaticDestructor s_staticDestructor;

    std::wstring m_longDirectoryPath;
    std::wstring m_longConfigFileName;
    std::wstring m_shortConfigFileName;

    HANDLE m_dirHandle;

    OVERLAPPED m_overlapped;
    HANDLE m_overlappedEvent;

    UINT64 LastReadTimestamp = 0;

    //
    // Must be DWORD aligned so allocated on the heap.
    //
    std::vector<BYTE> records = std::vector<BYTE>(8192);
};