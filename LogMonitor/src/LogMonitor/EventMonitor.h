//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

class EventMonitor final
{
public:
    EventMonitor() = delete;

    EventMonitor(
        _In_ const std::vector<EventLogChannel>& eventChannels,
        _In_ bool EventFormatMultiLine,
        _In_ bool StartAtOldestRecord,
        _In_ LoggerSettings& settings
        );

    ~EventMonitor();

private:
    static constexpr int EVENT_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS = 5 * 1000;
    static constexpr int EVENT_ARRAY_SIZE = 10;

    const std::vector<EventLogChannel> m_eventChannels;
    bool m_eventFormatMultiLine;
    bool m_startAtOldestRecord;
    LoggerSettings& m_settings;


    std::wstring source;
    std::wstring eventTime;
    std::wstring eventChannel;
    std::wstring eventLevel;
    UINT16 eventId;
    std::wstring eventMessage;

    //
    // Signaled by destructor to request the spawned thread to stop.
    //
    HANDLE m_stopEvent;

    //
    // Handle to an event subscriber thread.
    //
    HANDLE m_eventMonitorThread;

    std::vector<wchar_t> m_eventMessageBuffer;

    DWORD StartEventMonitor();

    static DWORD StartEventMonitorStatic(
        _In_ LPVOID Context
        );

    std::wstring ConstructWindowsEventQuery(
        _In_ const std::vector<EventLogChannel>& EventChannels
        );

    DWORD EnumerateResults(
        _In_ EVT_HANDLE ResultsHandle
        );

    DWORD PrintEvent(
        _In_ const HANDLE& EventHandle
        );

    void EnableEventLogChannels();

    static void EnableEventLogChannel(_In_ LPCWSTR ChannelPath);

    std::wstring XMLFormattedEvent();

    std::wstring LineFormattedEvent();

    std::wstring JSONFormattedEvent();
};
