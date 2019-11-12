//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#define DEFAULT_CONFIG_FILENAME L"C:\\LogMonitor\\LogMonitorConfig.json"

#define JSON_TAG_LOG_CONFIG     L"LogConfig"
#define JSON_TAG_SOURCES        L"sources"

///
/// Valid source attributes
///
#define JSON_TAG_TYPE                       L"type"
#define JSON_TAG_FORMAT_MULTILINE           L"eventFormatMultiLine"
#define JSON_TAG_START_AT_OLDEST_RECORD     L"startAtOldestRecord"
#define JSON_TAG_CHANNELS                   L"channels"
#define JSON_TAG_DIRECTORY                  L"directory"
#define JSON_TAG_FILTER                     L"filter"
#define JSON_TAG_INCLUDE_SUBDIRECTORIES     L"includeSubdirectories"
#define JSON_TAG_PROVIDERS                  L"providers"

///
/// Valid channel attributes
///
#define JSON_TAG_CHANNEL_NAME       L"name"
#define JSON_TAG_CHANNEL_LEVEL      L"level"

///
/// Valid ETW provider attributes
///
#define JSON_TAG_PROVIDER_NAME      L"providerName"
#define JSON_TAG_PROVIDER_GUID      L"providerGuid"
#define JSON_TAG_PROVIDER_LEVEL     L"level"
#define JSON_TAG_KEYWORDS           L"keywords"

///
/// Default values
///
#define EVENT_MONITOR_MULTILINE_DEFAULT                 true
#define EVENT_MONITOR_START_AT_OLDEST_RECORD_DEFAULT    false
#define ETW_MONITOR_MULTILINE_DEFAULT                   true


//
// Define the AttributesMap, that is a map<wstring, void*> with case
// insensitive keys
//
struct CaseInsensitiveWideString
{
    bool operator() (const std::wstring& c1, const std::wstring& c2) const {
        return _wcsicmp(c1.c_str(), c2.c_str()) < 0;
    }
};

typedef std::map<std::wstring, void*, CaseInsensitiveWideString> AttributesMap;

enum class EventChannelLogLevel
{
    Critical = 1,
    Error = 2,
    Warning = 3,
    Information = 4,
    Verbose = 5,
    All = 6,
};

DEFINE_ENUM_FLAG_OPERATORS(EventChannelLogLevel);

///
/// String names of the EventChannelLogLevel enum, used to parse the config file
///
const std::wstring LogLevelNames[] = {
    L"Critical",
    L"Error",
    L"Warning",
    L"Information",
    L"Verbose"
};

///
/// Array with EventChannelLogLevel enum values, used to get the real value,
/// when you have the index of the desired value at LogLevelNames.
///
const EventChannelLogLevel LogLevelValues[] = {
    EventChannelLogLevel::Critical,
    EventChannelLogLevel::Error,
    EventChannelLogLevel::Warning,
    EventChannelLogLevel::Information,
    EventChannelLogLevel::Verbose
};

inline
bool
StringToGuid(_In_ const std::wstring& str, _Out_ GUID& guid)
{
    std::wstring guidStr;

    //
    // Validate if is a GUID
    //
    if (str.length() == 36)
    {
        guidStr = str;
    }
    else if (str.length() == 38
        && str[0] == '{'
        && str[37] == '}')
    {
        guidStr = str.substr(1, 36);
    }
    else
    {
        return false;
    }

    for (int i = 0; i < 36; i++)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (guidStr[i] != '-')
            {
                return false;
            }
        }
        else if (!isxdigit(guidStr[i]))
        {
            return false;
        }
    }

    int count = swscanf_s(guidStr.c_str(),
        L"%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
        &guid.Data1, &guid.Data2, &guid.Data3,
        &guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
        &guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);

    //
    // Check if all the data was successfully read
    //
    return count == 11;
}

enum class LogSourceType
{
    EventLog = 0,
    File,
    ETW
};

///
/// String names of the LogSourceType enum, used to parse the config file
///
const LPCWSTR LogSourceTypeNames[] = {
    L"EventLog",
    L"File",
    L"ETW"
};

///
/// Base class of a generic source configuration.
/// it only include the type (used to recover the real type with polymorphism)
///
class LogSource
{
public:
    LogSourceType Type;
};

///
/// Information about an event log channel, It includes its name and Log level
///
typedef struct _EventLogChannel
{
    std::wstring Name;
    EventChannelLogLevel Level = EventChannelLogLevel::Error;

    inline bool IsValid()
    {
        return !Name.empty();
    }

    inline bool SetLevelByString(
        _In_ const std::wstring& Str
    )
    {
        int errorLevelSize = sizeof(LogLevelNames) / sizeof(LogLevelNames[0]);

        for (int i = 0; i < errorLevelSize; i++)
        {
            if (_wcsicmp(Str.c_str(), LogLevelNames[i].c_str()) == 0)
            {
                Level = LogLevelValues[i];
                return true;
            }
        }

        return false;
    }

    bool operator <(const _EventLogChannel& Other) const
    {
        int cmpNameResult = _wcsicmp(this->Name.c_str(), Other.Name.c_str());
        return cmpNameResult < 0 || (cmpNameResult == 0 && this->Level < Other.Level);
    }

    bool operator ==(const _EventLogChannel& Other) const
    {
        return _wcsicmp(this->Name.c_str(), Other.Name.c_str()) == 0 && this->Level == Other.Level;
    }
} EventLogChannel;

///
/// Represents a Source of EventLog type
///
class SourceEventLog : LogSource
{
public:
    std::vector<EventLogChannel> Channels;
    std::shared_ptr<bool> EventFormatMultiLine = nullptr;
    std::shared_ptr<bool> StartAtOldestRecord = nullptr;

    static bool Unwrap(
        _In_ AttributesMap& Attributes,
        _Out_ SourceEventLog& NewSource)
    {
        NewSource.Type = LogSourceType::EventLog;

        //
        // Get required 'channels' value
        //
        if (Attributes.find(JSON_TAG_CHANNELS) == Attributes.end()
            || Attributes[JSON_TAG_CHANNELS] == nullptr)
        {
            return false;
        }

        //
        // Clone the array, the original one could be deleted.
        //
        NewSource.Channels = *(std::vector<EventLogChannel>*)Attributes[JSON_TAG_CHANNELS];

        //
        // eventFormatMultiLine is an optional value
        //
        if (Attributes.find(JSON_TAG_FORMAT_MULTILINE) != Attributes.end()
            && Attributes[JSON_TAG_FORMAT_MULTILINE] != nullptr)
        {
            NewSource.EventFormatMultiLine = std::make_shared<bool>(*(bool*)Attributes[JSON_TAG_FORMAT_MULTILINE]);
        }

        //
        // startAtOldestRecord is an optional value
        //
        if (Attributes.find(JSON_TAG_START_AT_OLDEST_RECORD) != Attributes.end()
            && Attributes[JSON_TAG_START_AT_OLDEST_RECORD] != nullptr)
        {
            NewSource.StartAtOldestRecord = std::make_shared<bool>(*(bool*)Attributes[JSON_TAG_START_AT_OLDEST_RECORD]);
        }

        return true;
    }
};

///
/// Represents a Source of File type
///
class SourceFile : LogSource
{
public:
    std::wstring Directory;
    std::wstring Filter;
    bool IncludeSubdirectories = false;

    static bool Unwrap(
        _In_ AttributesMap& Attributes,
        _Out_ SourceFile& NewSource)
    {
        NewSource.Type = LogSourceType::File;

        //
        // Directory is required
        //
        if (Attributes.find(JSON_TAG_DIRECTORY) == Attributes.end()
            || Attributes[JSON_TAG_DIRECTORY] == nullptr)
        {
            return false;
        }

        //
        // Clone the value, the original one could be deleted.
        //
        NewSource.Directory = *(std::wstring*)Attributes[JSON_TAG_DIRECTORY];

        //
        // Filter is an optional value
        //
        if (Attributes.find(JSON_TAG_FILTER) != Attributes.end()
            && Attributes[JSON_TAG_FILTER] != nullptr)
        {
            NewSource.Filter = *(std::wstring*)Attributes[JSON_TAG_FILTER];
        }

        //
        // includeSubdirectories is an optional value
        //
        if (Attributes.find(JSON_TAG_INCLUDE_SUBDIRECTORIES) != Attributes.end()
            && Attributes[JSON_TAG_INCLUDE_SUBDIRECTORIES] != nullptr)
        {
            NewSource.IncludeSubdirectories = *(bool*)Attributes[JSON_TAG_INCLUDE_SUBDIRECTORIES];
        }

        return true;
    }
};

///
/// ETW Provider
///
class ETWProvider
{
public:
    std::wstring ProviderName;
    std::wstring ProviderGuidStr;
    GUID ProviderGuid = { 0 };
    ULONGLONG Keywords = 0;
    UCHAR Level = 2; // Error level 

    inline bool IsValid()
    {
        return !ProviderName.empty() || !ProviderGuidStr.empty();
    }

    inline bool SetProviderGuid(const std::wstring &value)
    {
        GUID guid;

        if (!StringToGuid(value, guid))
        {
            return false;
        }

        ProviderGuid = guid;
        ProviderGuidStr = value;

        return true;
    }

    inline bool StringToLevel(
        _In_ const std::wstring& Str
    )
    {
        int errorLevelSize = (sizeof(LogLevelNames) / sizeof(LogLevelNames[0])); // Don't include the ALL value

        for (UCHAR i = 0; i < errorLevelSize; i++)
        {
            if (_wcsicmp(Str.c_str(), LogLevelNames[i].c_str()) == 0)
            {
                //
                // Level starts at 1
                //
                Level = i + 1;
                return true;
            }
        }

        return false;
    }

    bool operator <(const ETWProvider& Other) const
    {
        int cmpGuidResult = memcmp(&ProviderGuid, &Other.ProviderGuid, sizeof(ProviderGuid));
        if (cmpGuidResult != 0)
        {
            return cmpGuidResult < 0;
        }

        int cmpNameResult = _wcsicmp(this->ProviderGuidStr.c_str(), Other.ProviderGuidStr.c_str());
        return cmpNameResult < 0 || 
            (cmpNameResult == 0 && this->Level < Other.Level) ||
            (cmpNameResult == 0 && this->Level == Other.Level && this->Keywords < Other.Keywords);
    }

    bool operator ==(const ETWProvider& Other) const
    {
        return memcmp(&ProviderGuid, &Other.ProviderGuid, sizeof(ProviderGuid)) == 0 &&
            _wcsicmp(this->ProviderGuidStr.c_str(), Other.ProviderGuidStr.c_str()) == 0 &&
            this->Level == Other.Level &&
            this->Keywords == Other.Keywords;
    }
};

///
/// Represents a Source if ETW type
///
class SourceETW : LogSource
{
public:
    std::vector<ETWProvider> Providers;
    std::shared_ptr<bool> EventFormatMultiLine = nullptr;

    static bool Unwrap(
        _In_ AttributesMap& Attributes,
        _Out_ SourceETW& NewSource)
    {
        NewSource.Type = LogSourceType::ETW;

        //
        // Get required 'providers' value
        //
        if (Attributes.find(JSON_TAG_PROVIDERS) == Attributes.end()
            || Attributes[JSON_TAG_PROVIDERS] == nullptr)
        {
            return false;
        }

        //
        // Clone the array, the original one could be deleted.
        //
        NewSource.Providers = *(std::vector<ETWProvider>*)Attributes[JSON_TAG_PROVIDERS];

        //
        // eventFormatMultiLine is an optional value
        //
        if (Attributes.find(JSON_TAG_FORMAT_MULTILINE) != Attributes.end()
            && Attributes[JSON_TAG_FORMAT_MULTILINE] != nullptr)
        {
            NewSource.EventFormatMultiLine = std::make_shared<bool>(*(bool*)Attributes[JSON_TAG_FORMAT_MULTILINE]);
        }


        return true;
    }
};

///
/// Information about a channel Log
///
typedef struct _LoggerSettings
{
    struct _Sources
    {
        std::shared_ptr<SourceEventLog> EventLog = nullptr;
        std::vector<std::shared_ptr<SourceFile>> LogFiles;
        std::shared_ptr<SourceETW> ETW = nullptr;
    } Sources;

    _LoggerSettings()
    {
        Sources.EventLog = nullptr;
        Sources.ETW = nullptr;
    }

    _LoggerSettings(const std::vector<std::shared_ptr<LogSource>>& NewSources)
    {
        for(auto source : NewSources)
        {
            switch (source->Type)
            {
                case LogSourceType::EventLog:
                {
                    std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(source);

                    if (Sources.EventLog == nullptr)
                    {
                        Sources.EventLog = std::make_shared<SourceEventLog>(*sourceEventLog);
                    }
                    else
                    {
                        for (auto channel : sourceEventLog->Channels)
                        {
                            Sources.EventLog->Channels.push_back(channel);
                        }

                        if (sourceEventLog->EventFormatMultiLine != nullptr)
                        {
                            Sources.EventLog->EventFormatMultiLine = sourceEventLog->EventFormatMultiLine;
                        }

                        if (sourceEventLog->StartAtOldestRecord != nullptr)
                        {
                            Sources.EventLog->StartAtOldestRecord = sourceEventLog->StartAtOldestRecord;
                        }
                    }

                    break;
                }
                case LogSourceType::File:
                {
                    std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(source);

                    Sources.LogFiles.push_back(sourceFile);

                    break;
                }
                case LogSourceType::ETW:
                {
                    std::shared_ptr<SourceETW> sourceETW = std::reinterpret_pointer_cast<SourceETW>(source);

                    if (Sources.ETW == nullptr)
                    {
                        Sources.ETW = std::make_shared<SourceETW>(*sourceETW);;
                    }
                    else
                    {
                        for (auto provider : sourceETW->Providers)
                        {
                            Sources.ETW->Providers.push_back(provider);
                        }

                        if (sourceETW->EventFormatMultiLine != nullptr)
                        {
                            Sources.ETW->EventFormatMultiLine = sourceETW->EventFormatMultiLine;
                        }
                    }

                    break;
                }
            }
        }
    }
} LoggerSettings;