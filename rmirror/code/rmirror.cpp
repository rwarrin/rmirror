/**
 * NOTE(rick):
 *
 * - Known Issues
 *   - None!
 **/

#define REQUIRED_FILE_NAME_IGNORE_SLOTS 1

struct rmirror_config_parser
{
    u32 LineCount;
    wchar *Lines[32];
};

internal void
CleanLine(char *At)
{
    while(*At)
    {
        if((*At == '\r') ||
           (*At == '\n'))
        {
            *At = 0;
        }
        ++At;
    }
}

internal void
WCleanLine(wchar *At)
{
    while(*At)
    {
        if((*At == L'\r') ||
           (*At == L'\n'))
        {
            *At = L'\0';
        }
        ++At;
    }
}

inline void
ScanToChar(char **ScanIn, char Target)
{
    char *At = *ScanIn;
    for(;;)
    {
        if((*At == 0) ||
           (*At == Target))
        {
            break;
        }

        ++At;
    }

    *ScanIn = At;
}

inline void
ScanToWChar(wchar **ScanIn, wchar Target)
{
    wchar *At = *ScanIn;
    for(;;)
    {
        if((*At == L'\0') ||
           (*At == Target))
        {
            break;
        }

        ++At;
    }

    *ScanIn = At;
}

inline wchar *
WFindCharacter(wchar *String, wchar C)
{
    wchar *Result = 0;

    wchar *At = String;
    while(*At)
    {
        if(*At == C)
        {
            Result = At;
            break;
        }
        ++At;
    }

    return(Result);
}

internal wchar *
GetOutputFileName(memory_arena *Arena, rmirror_config *Config)
{
    time_t TimeNow = {0};
    time(&TimeNow);
    tm *TimeInfo = localtime(&TimeNow);

    wchar *OutputFileName = Config->OutputFileName;

    u32 OriginalNameLength = WStringLength(OutputFileName);
    wchar *ExtensionOffset = WFindCharacter(OutputFileName, L'.');
    b32 HasExtension = (ExtensionOffset[0] != L'\0');
    u32 NameOnlyLength = ExtensionOffset - OutputFileName;

    wchar *Result = PushArray(Arena, wchar, MAX_PATH);
    s32 BytesWritten = _snwprintf(Result, MAX_PATH, L"%.*s", NameOnlyLength, OutputFileName);

    if(TimeInfo && Config->AppendDateToFileName)
    {
        BytesWritten += _snwprintf(Result + BytesWritten, MAX_PATH - BytesWritten,
                                   L"_%04d%02d%02d",
                                   TimeInfo->tm_year + 1900,
                                   TimeInfo->tm_mon + 1,
                                   TimeInfo->tm_mday);
    }
    if(TimeInfo && Config->AppendTimeToFileName)
    {
        BytesWritten += _snwprintf(Result + BytesWritten, MAX_PATH - BytesWritten,
                                   L"_%02d%02d%02d",
                                   TimeInfo->tm_hour,
                                   TimeInfo->tm_min,
                                   TimeInfo->tm_sec);
    }

    if(HasExtension)
    {
        BytesWritten += _snwprintf(Result + BytesWritten, MAX_PATH - BytesWritten,
                                   L"%s", ExtensionOffset);
    }

    return(Result);
}

internal wchar *
GetOutputFileFullPath(memory_arena *Arena, rmirror_config *Config)
{
    wchar *Result = PushArray(Arena, wchar, MAX_PATH);
    wchar *FileName = Config->ArchiveFileName;
    _snwprintf(Result, MAX_PATH, L"%s/%s", Config->RootPath, FileName);

    return(Result);
}

internal wchar *
GetDestinationFullPath(memory_arena *Arena, rmirror_config *Config)
{
    wchar *Result = PushArray(Arena, wchar, MAX_PATH);
    s32 BytesWritten = _snwprintf(Result, MAX_PATH, L"%s/%s",
                                  Config->MoveToPath, Config->ArchiveFileName);
    return(Result);
}

internal void
ParseConfig_FileName(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    wchar *At = Line;
    ScanToWChar(&At, L'=');
    ++At;

    u32 Length = WStringLength(At);
    wchar *Buffer = PushArray(Arena, wchar, Length + 1);
    ZeroArray(Buffer, Length + 1);

    Copy(Length*sizeof(*At), At, Buffer);
    Config->OutputFileName = Buffer;
}

internal b32
ParseBooleanEntry(wchar *Line)
{
    b32 Result = 0;

    wchar *At = Line;
    ScanToWChar(&At, L'=');
    ++At;

    if((At[0] == L'1') ||
       (At[0] == L'T') || (At[0] == L't') ||
       (At[0] == L'Y') || (At[0] == L'y'))
    {
        Result = true;
    }
    
    return(Result);
}

internal void
ParseConfig_AppendDate(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    Config->AppendDateToFileName = ParseBooleanEntry(Line);
}

internal void
ParseConfig_AppendTime(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    Config->AppendTimeToFileName = ParseBooleanEntry(Line);
}

internal void
ParseConfig_MoveTo(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    wchar *At = Line;
    ScanToWChar(&At, L'=');
    ++At;

    u32 Length = WStringLength(At);
    wchar *Buffer = PushArray(Arena, wchar, Length + 1);
    ZeroArray(Buffer, Length + 1);

    Copy(Length*sizeof(*At), At, Buffer);
    Config->MoveOutputFile = true;
    Config->MoveToPath = Buffer;
}

internal void
ParseCommaSeparatedList(memory_arena *Arena, wchar *Line,
                        wchar **DestArray, u32 MaxArraySize, u32 *ArrayCount)
{
    wchar *At = Line;
    ScanToWChar(&At, L'=');
    ++At;

    u32 Length = WStringLength(At) + 1;
    wchar *Buffer = PushArray(Arena, wchar, Length);
    ZeroArray(Buffer, Length);
    Copy(Length*sizeof(*At), At, Buffer);

    At = Buffer;
    wchar *ValueBegin = Buffer;
    while(Length--)
    {
        b32 FoundValue = false;
        if((*At == L',') ||
           (*At == L'\0'))
        {
            *At = L'\0';
            FoundValue = true;
        }

        ++At;

        if(FoundValue)
        {
            if((*ArrayCount + 1) < MaxArraySize)
            {
                DestArray[(*ArrayCount)++] = ValueBegin;
            }
            else
            {
                fwprintf(stderr, L"Too many ignored values. Skipping '%s'\n", ValueBegin);
                Assert(!"Not enough space in array!");
                break;
            }

            ValueBegin = At;
        }
    }
}

internal void
ParseConfig_IgnoreFileTypes(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    ParseCommaSeparatedList(Arena, Line,
                            Config->ExtensionsToIgnore,
                            ArrayCount(Config->ExtensionsToIgnore),
                            &Config->ExtensionsToIgnoreCount);
}

internal void
ParseConfig_IgnoreDirs(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    ParseCommaSeparatedList(Arena, Line,
                            Config->DirectoriesToIgnore,
                            ArrayCount(Config->DirectoriesToIgnore),
                            &Config->DirectoriesToIgnoreCount);
}

internal void
ParseConfig_IgnoreFileNames(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    ParseCommaSeparatedList(Arena, Line,
                            Config->FileNamesToIgnore,
                            ArrayCount(Config->FileNamesToIgnore) - REQUIRED_FILE_NAME_IGNORE_SLOTS,
                            &Config->FileNamesToIgnoreCount);
}

internal void
ParseConfig_OverwriteExistingBackup(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    Config->ReplaceExistingFile = ParseBooleanEntry(Line);
}

internal void
ParseConfig_BackupsToKeep(memory_arena *Arena, wchar *Line, rmirror_config *Config)
{
    wchar *At = Line;
    ScanToWChar(&At, L'=');
    ++At;

    Config->BackupsToKeep = (u32)wcstol(At, 0, 10);
}

internal void
ParseConfig_SetRootPath(memory_arena *Arena, char *Path, rmirror_config *Config)
{
    wchar *WPath = CStrToWCStr(Arena, Path);
    Config->RootPath = WPath;
}

internal wchar *
GetBadConfigurationName(memory_arena *Arena, wchar *Line)
{
    wchar *At = Line;
    ScanToWChar(&At, L'=');
    *At = L'\0';

    u32 Length = WStringLength(Line);
    wchar *Temp = PushArray(Arena, wchar, Length);
    Copy(Length*sizeof(*Line), Line, Temp);
    return(Temp);
}

inline b32
ValidateConfig(rmirror_config *Config)
{
    b32 Result = ((Config->RootPath != L'\0') &&
                  (Config->OutputFileName != L'\0') &&
                  (Config->ArchiveFileName != L'\0'));

    return(Result);
}

internal b32
ReadConfigFromFile(memory_arena *Arena, rmirror_config *Config, char *WorkDirectory)
{
    b32 Result = false;
    if(Config)
    {
        char *ConfigPath = PushArray(Arena, char, MAX_PATH);
        _snprintf(ConfigPath, MAX_PATH, "%s/.rmirror", WorkDirectory);
        FILE *ConfigFile = fopen(ConfigPath, "rb");
        if(ConfigFile)
        {
            rmirror_config_parser *Parser = PushStruct(Arena, rmirror_config_parser);
            ZeroStruct(*Parser);

            char ReadBufferRaw[1024] = {0};
            wchar ReadBuffer[1024] = {0};
            while(fgets(ReadBufferRaw, ArrayCount(ReadBufferRaw), ConfigFile) != 0)
            {
                mbstowcs(ReadBuffer, ReadBufferRaw, ArrayCount(ReadBufferRaw));

                if((ReadBuffer[0] == L'\r') ||
                   (ReadBuffer[0] == L'\n') ||
                   (ReadBuffer[0] == L'#'))
                {
                    // NOTE(rick): Skip empty lines and comments
                    continue;
                }

                u32 Length = WStringLength(ReadBuffer);
                wchar *CopyBuffer = PushArray(Arena, wchar, (Length + 1));
                Copy(Length*sizeof(*ReadBuffer), ReadBuffer, CopyBuffer);
                CopyBuffer[Length] = 0;

                if((Parser->LineCount + 1) < ArrayCount(Parser->Lines))
                {
                    Parser->Lines[Parser->LineCount++] = CopyBuffer;
                }
                else
                {
                    Assert(!"Too many lines!");
                }
            }

            fclose(ConfigFile);

            ParseConfig_SetRootPath(Arena, WorkDirectory, Config);
            for(u32 LineIndex = 0; LineIndex < Parser->LineCount; ++LineIndex)
            {
                wchar *Line = *(Parser->Lines + LineIndex);
                WCleanLine(Line);

                if(WStringsMatch(L"file_name", Line))
                {
                    ParseConfig_FileName(Arena, Line, Config);
                }
                else if(WStringsMatch(L"append_date", Line))
                {
                    ParseConfig_AppendDate(Arena, Line, Config);
                }
                else if(WStringsMatch(L"append_time", Line))
                {
                    ParseConfig_AppendTime(Arena, Line, Config);
                }
                else if(WStringsMatch(L"move_to", Line))
                {
                    ParseConfig_MoveTo(Arena, Line, Config);
                }
                else if(WStringsMatch(L"ignore_file_types", Line))
                {
                    ParseConfig_IgnoreFileTypes(Arena, Line, Config);
                }
                else if(WStringsMatch(L"ignore_dirs", Line))
                {
                    ParseConfig_IgnoreDirs(Arena, Line, Config);
                }
                else if(WStringsMatch(L"backups_to_keep", Line))
                {
                    ParseConfig_BackupsToKeep(Arena, Line, Config);
                }
                else if(WStringsMatch(L"ignore_files", Line))
                {
                    ParseConfig_IgnoreFileNames(Arena, Line, Config);
                }
                else if(WStringsMatch(L"overwrite_existing_backup", Line))
                {
                    ParseConfig_OverwriteExistingBackup(Arena, Line, Config);
                }
                else
                {
                    temp_memory TempMem = BeginTemporaryMemory(Arena);
                    fwprintf(stderr, L"Unknown configuration option: '%s'\n",
                            GetBadConfigurationName(Arena, Line));
                    EndTemporaryMemory(TempMem);
                }
            }

            if(!Config->MoveOutputFile)
            {
                Config->MoveToPath = Config->RootPath;
            }

            Config->ArchiveFileName = GetOutputFileName(Arena, Config);

            b32 ConfigIsValid = ValidateConfig(Config);
            Config->Valid = ConfigIsValid;
            Result = ConfigIsValid;
        }
    }

    return(Result);
}

internal void
GetDefaultConfig(memory_arena *Arena, rmirror_config *Config, char *Path)
{
    if(Config)
    {
        wchar *WPath = CStrToWCStr(Arena, Path);
        Config->RootPath = WPath;
        Config->OutputFileName = PushWString(Arena, L"rmirror_backup.zip");
        Config->ArchiveFileName = GetOutputFileName(Arena, Config);
        Config->MoveOutputFile = false;
        Config->BackupsToKeep = 0;
        Config->ReplaceExistingFile = true;

        Config->Valid = ValidateConfig(Config);
        Assert(Config->Valid);
    }
}

inline wchar *
FindLastOfCharacter(wchar *String, wchar C)
{
    wchar *Result = 0;

    wchar *At = String;
    while(*At)
    {
        if(*At == C)
        {
            Result = At;
        }
        ++At;
    }

    return(Result);
}

internal void
AddApplicationDefinedFilters(memory_arena *Arena, rmirror_config *Config)
{
    Assert((Config->FileNamesToIgnoreCount + REQUIRED_FILE_NAME_IGNORE_SLOTS) <
           ArrayCount(Config->FileNamesToIgnore));

    Config->FileNamesToIgnore[Config->FileNamesToIgnoreCount++] = Config->OutputFileName;
}

internal void
FilterFileList(memory_arena *Arena, rmirror_config *Config, file_list *FileList)
{
    for(u32 FileIndex = 0; FileIndex < FileList->Count; ++FileIndex)
    {
        wchar *File = FileList->Files[FileIndex];
        b32 RemoveFile = false;

        wchar *Name = FindLastOfCharacter(File, L'/');
        wchar *Extension = FindLastOfCharacter(Name, L'.');
        wchar *FullPath = File;
        if(Extension)
        {
            for(u32 ExtensionIndex = 0;
                !RemoveFile && (ExtensionIndex < Config->ExtensionsToIgnoreCount);
                ++ExtensionIndex)
            {
                wchar *IgnoredExtension = *(Config->ExtensionsToIgnore + ExtensionIndex);
                if(WStringsMatch(IgnoredExtension, Extension))
                {
                    RemoveFile = true;
                    break;
                }
            }
        }
        if(Name)
        {
            ++Name; // NOTE(rick): Move past the found '/'
            for(u32 NameIndex = 0;
                !RemoveFile && (NameIndex < Config->FileNamesToIgnoreCount);
                ++NameIndex)
            {
                wchar *IgnoredName = Config->FileNamesToIgnore[NameIndex];
                if(WStringsMatch(IgnoredName, Name))
                {
                    RemoveFile = true;
                    break;
                }
            }
        }
        if(FullPath)
        {
            for(u32 DirectoryIndex = 0;
                !RemoveFile && (DirectoryIndex < Config->DirectoriesToIgnoreCount);
                ++DirectoryIndex)
            {
                wchar *IgnoredDirectory = Config->DirectoriesToIgnore[DirectoryIndex];
                if(WStringFind(FullPath, IgnoredDirectory))
                {
                    RemoveFile = true;
                    break;
                }
            }
        }

        if(RemoveFile)
        {
            fwprintf(stderr, L"Ignoring file '%s'\n", File);
            FileList->Flags[FileIndex] |= FileListFlag_Exclude;
            ++FileList->FilteredCount;
        }
    }
}

internal void
CreateArchiveFromFileList(memory_arena *Arena, rmirror_config *Config, file_list *FileList)
{
    Assert(GetTimeTFromFile);

    temp_memory TempMem = BeginTemporaryMemory(Arena);

    wchar *OutputNameW = GetOutputFileFullPath(Arena, Config);
    char *OutputName = PushArray(Arena, char, MAX_PATH);
    WideCharToMultiByte(CP_ACP, 0, OutputNameW, -1, OutputName, MAX_PATH, 0, 0);
    mz_zip_archive zip_archive = {0};
    mz_zip_writer_init_file(&zip_archive, OutputName, 0);
    
    u32 LastPercentOutput = -1;
    for(u32 FileIndex = 0;
        FileIndex < FileList->Count;
        ++FileIndex)
    {
        u32 PercentComplete = (u32)(((f32)FileIndex / (f32)FileList->Count)*100.0f);
        if(((PercentComplete % 10) == 0) &&
           (PercentComplete != LastPercentOutput))
        {
            printf("  %3u%%\n", PercentComplete);
            LastPercentOutput = PercentComplete;
        }

        if(!(FileList->Flags[FileIndex] & FileListFlag_Exclude))
        {
            wchar *FileName = FileList->Files[FileIndex];
            u32 RootPathLength = WStringLength(Config->RootPath);
            wchar *ArchiveNameW = FileName + RootPathLength;

            if((FileName != L'\0') &&
               (ArchiveNameW != L'\0'))
            {
                ArchiveNameW++;

                temp_memory FileTempMem = BeginTemporaryMemory(Arena);
                char *ArchiveName = PushArray(Arena, char, MAX_PATH);

                int Converted = WideCharToMultiByte(CP_ACP, 0, ArchiveNameW, -1,
                                                    ArchiveName, MAX_PATH, 0, 0);
                if(Converted != 0)
                {
                    FILE *File = _wfopen(FileName, L"rb");
                    if(File)
                    {
                        _fseeki64(File, 0, SEEK_END);
                        u64 FileSize = _ftelli64(File);
                        _fseeki64(File, 0, SEEK_SET);

                        time_t TimeT = {0};
                        if(GetTimeTFromFile(File, &TimeT))
                        {
                            mz_zip_writer_add_cfile(&zip_archive, ArchiveName, File, FileSize, &TimeT, 0, 0, MZ_BEST_COMPRESSION, 0, 0, 0, 0, 0);
                        }
                        else
                        {
                            wprintf(L"Failed to get last write time for file '%s'\n", FileName);
                        }

                        fclose(File);
                    }
                    else
                    {
                        wprintf(L"Failed to open file '%s'\n", FileName);
                    }
                }
                else
                {
                    wprintf(L"Failed to convert file name '%s'\n", FileName);
                }

                EndTemporaryMemory(FileTempMem);
            }
        }
    }
    printf("  %3u%%\n", 100);

    mz_zip_writer_finalize_archive(&zip_archive);
    mz_zip_writer_end(&zip_archive);

    EndTemporaryMemory(TempMem);
}
