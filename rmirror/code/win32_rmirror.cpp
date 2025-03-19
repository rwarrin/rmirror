
#include <windows.h>
#include <direct.h>

#include "rmirror_platform.h"
#include "rmirror.h"

#include "rmirror.cpp"

internal void
PrintHelp(FILE *Out, char *Command)
{
    char *FormatString = "  %-16s%-62s\n";
    fprintf(Out, "Usage: %s [path]\n", Command);
    fprintf(Out, FormatString, "path", "Path that the application should find a .rmirror file.");
    fprintf(Out, FormatString, "-h", "Prints this message.");
}

#define FILE_VISITOR(Name) void (Name)(WIN32_FIND_DATAW *FindData, wchar *Path, void *CustomData)
typedef FILE_VISITOR(file_visitor);
internal void
Win32VisitPathRecursive(wchar_t *RootPath, file_visitor *Visitor, void *CustomData, memory_arena *Arena)
{
    WIN32_FIND_DATAW FindData = {0};
    HANDLE FindHandle = FindFirstFileExW(RootPath, FindExInfoBasic,
                                         &FindData, FindExSearchNameMatch,
                                         0, FIND_FIRST_EX_LARGE_FETCH);
    while(FindHandle != INVALID_HANDLE_VALUE)
    {
        if((FindData.cFileName[0] == L'.') &&
           ((FindData.cFileName[1] == L'.') || (FindData.cFileName[1] == L'\0')))
        {
            // NOTE(rick): Skip . and ..
        }
        else if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            temp_memory Temp = BeginTemporaryMemory(Arena);

            u32 PathLength = WStringLength(RootPath);
            wchar *NewPath = PushArray(Arena, wchar, MAX_PATH);
            _snwprintf(NewPath, MAX_PATH, L"%.*s/%s/*",
                       PathLength - 2, RootPath, FindData.cFileName);
            Win32VisitPathRecursive(NewPath, Visitor, CustomData, Arena);

            EndTemporaryMemory(Temp);
        }
        else
        {
            Visitor(&FindData, RootPath, CustomData);
        }

        if(!FindNextFileW(FindHandle, &FindData))
        {
            break;
        }
    }
}

static u32 *GlobalFileCounterPtr = 0;
internal FILE_VISITOR(FileCountVisitor)
{
    ++(*GlobalFileCounterPtr);
}

internal u32
Win32GetFileCount(wchar *Path)
{
    memory_arena *Scratch = make_scratch(Scratch, 0);

    u32 FileCount = 0;
    GlobalFileCounterPtr = &FileCount;

    wchar *SearchString = PushArray(Scratch, wchar, MAX_PATH);
    _snwprintf(SearchString, MAX_PATH, L"%s/*", Path);
    Win32VisitPathRecursive(SearchString, FileCountVisitor, 0, Scratch);

    GlobalFileCounterPtr = 0;
    return(FileCount);
}

internal FILE_VISITOR(BuildFileListVisitor)
{
    file_list *FileList = (file_list *)CustomData;

    Assert((FileList->Count + 1) <= FileList->MaxCount);

    u32 PathLength = WStringLength(Path);
    u32 FileNameLength = WStringLength(FindData->cFileName);
    u32 TotalLength = PathLength+FileNameLength+1;

    wchar *FileNameBuffer = PushArray(FileList->Arena, wchar, TotalLength);
    _snwprintf(FileNameBuffer, MAX_PATH, L"%.*s/%s\0", PathLength - 2, Path, FindData->cFileName);
    FileList->Files[FileList->Count++] = FileNameBuffer;
}

internal file_list *
BeginFileList(rmirror_config *Config)
{
    u32 MaxFileCount = Win32GetFileCount(Config->RootPath);

    mem_size FileNamePtrArraySize = MaxFileCount * sizeof(wchar *);
    mem_size FileNameMemorySize = MaxFileCount*sizeof(wchar)*(MAX_PATH + 1);
    mem_size FileFlagsMemorySize = MaxFileCount*sizeof(u8);
    mem_size FilesArenaSize = FileNamePtrArraySize + FileNameMemorySize + FileFlagsMemorySize + sizeof(file_list);
    memory_arena *FilesArena = CreateArena(FilesArenaSize, 0, ArenaCreation_RoundToNextPowerOfTwo);

    file_list *FileList = PushStruct(FilesArena, file_list);
    FileList->Arena = FilesArena;
    FileList->Count = 0;
    FileList->MaxCount = MaxFileCount;
    FileList->FilteredCount = 0;
    FileList->Flags = PushArray(FilesArena, u8, FileList->MaxCount);
    FileList->Files = PushArray(FilesArena, wchar *, FileList->MaxCount);

    ZeroArray(FileList->Flags, sizeof(FileList->Files[0])*FileList->MaxCount);

    memory_arena *Scratch = make_scratch(Scratch, 0);
    wchar *SearchString = PushArray(Scratch, wchar, MAX_PATH);
    _snwprintf(SearchString, MAX_PATH, L"%s/*", Config->RootPath);
    Win32VisitPathRecursive(SearchString, BuildFileListVisitor, FileList, Scratch);

    return(FileList);
}

internal void
EndFileList(file_list *List)
{
    if(List)
    {
        List->Count = 0;
        List->MaxCount = 0;
        List->Files = 0;
        DestroyArena(List->Arena);
    }
}

internal b32
Win32MoveFile(rmirror_config *Config, wchar *Source, wchar *Dest)
{
    u32 MoveFileFlags = MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH;
    if(Config && Config->ReplaceExistingFile)
    {
        MoveFileFlags |= MOVEFILE_REPLACE_EXISTING;
    }

    b32 Result = MoveFileExW(Source, Dest, MoveFileFlags);
    return(Result);
}

internal void
Win32LimitNumberOfBackups(memory_arena *Arena, rmirror_config *Config)
{
    temp_memory TempMem = BeginTemporaryMemory(Arena);

    wchar *FileName = PushWString(Arena, Config->OutputFileName);
    wchar *Extension = FindLastOfCharacter(FileName, L'.');
    *Extension = L'\0';

    u32 FoundBackupCount = 0;
    FILETIME OldestFileTime = {0};
    wchar *OldestFileName = {0};

    wchar *SearchString = PushArray(Arena, wchar, MAX_PATH);
    _snwprintf(SearchString, MAX_PATH, L"%s/*", Config->MoveToPath);
    WIN32_FIND_DATAW FindData = {0};
    HANDLE FindHandle = FindFirstFileExW(SearchString, FindExInfoBasic,
                                         &FindData, FindExSearchNameMatch,
                                         0, FIND_FIRST_EX_LARGE_FETCH);
    b32 FoundMatchingFileName = false;
    while(FindHandle != INVALID_HANDLE_VALUE)
    {
        if((FindData.cFileName[0] == '.') &&
           ((FindData.cFileName[1] == '.') || (FindData.cFileName[1] == 0)))
        {
            // NOTE(rick): Skip . and ..
        }
        else if(FindData.dwFileAttributes != FILE_ATTRIBUTE_DIRECTORY)
        {
            if(WStringFind(FindData.cFileName, FileName))
            {
                ++FoundBackupCount;
                if(!FoundMatchingFileName)
                {
                    OldestFileTime = FindData.ftCreationTime;
                    OldestFileName = PushWString(Arena, FindData.cFileName);
                    FoundMatchingFileName = true;
                }

                if(CompareFileTime(&FindData.ftCreationTime, &OldestFileTime) < 0)
                {
                    OldestFileTime = FindData.ftCreationTime;
                    OldestFileName = PushWString(Arena, FindData.cFileName);
                }
            }
        }

        if(!FindNextFileW(FindHandle, &FindData))
        {
            break;
        }
    }

    if(FoundBackupCount >= Config->BackupsToKeep)
    {
        wchar *RemoveFileName = PushArray(Arena, wchar, MAX_PATH);
        _snwprintf(RemoveFileName, MAX_PATH, L"%s/%s", Config->MoveToPath, OldestFileName);
        DeleteFileW(RemoveFileName);
        --FoundBackupCount;
    }

    EndTemporaryMemory(TempMem);

    if(FoundBackupCount >= Config->BackupsToKeep)
    {
        Win32LimitNumberOfBackups(Arena, Config);
    }
}

GET_TIME_T_FROM_FILE(Win32GetTimeTFromFile)
{
    b32 Result = false;

    HANDLE Handle = (HANDLE)_get_osfhandle(_fileno(File));
    FILETIME FileTimeCreation = {0};
    FILETIME FileTimeLastAccess = {0};
    FILETIME FileTimeLastWrite = {0};
    if(GetFileTime(Handle, &FileTimeCreation, &FileTimeLastAccess, &FileTimeLastWrite))
    {
        ULARGE_INTEGER UInt;
        UInt.LowPart = FileTimeLastWrite.dwLowDateTime;
        UInt.HighPart = FileTimeLastWrite.dwHighDateTime;
        *TimeT = (time_t)((UInt.QuadPart - 116444736000000000ULL) / 10000000ULL);

        Result = true;
    }

    return(Result);
}
extern get_time_t_from_file *GetTimeTFromFile = Win32GetTimeTFromFile;

int main(s32 ArgCount, char **Args)
{
    if((ArgCount != 2) ||
       StringsMatch("-h", Args[1]))
    {
        PrintHelp(stdout, Args[0]);
        return(1);
    }


    memory_arena *PermanentArena = CreateArena(Megabytes(2));
    char *WorkDirectory = Args[1];
    rmirror_config Config = {0};
    if(!ReadConfigFromFile(PermanentArena, &Config, WorkDirectory))
    {
        GetDefaultConfig(PermanentArena, &Config, WorkDirectory);
    }

    printf("Building file list.\n");
    file_list *FileList = BeginFileList(&Config);

    printf("Applying filters.\n");
    AddApplicationDefinedFilters(PermanentArena, &Config);
    FilterFileList(PermanentArena, &Config, FileList);

    printf("Found %d files. Excluding %d files. Total %d\n",
           FileList->Count,
           FileList->FilteredCount,
           FileList->Count - FileList->FilteredCount);

    printf("Creating backup.\n");
    CreateArchiveFromFileList(PermanentArena, &Config, FileList);

    EndFileList(FileList);

    if(Config.BackupsToKeep > 0)
    {
        printf("Limiting number of backups.\n");
        Win32LimitNumberOfBackups(PermanentArena, &Config);
    }

    if(Config.MoveOutputFile)
    {
        printf("Moving backup.\n");
        temp_memory TempMem = BeginTemporaryMemory(PermanentArena);
        Win32MoveFile(&Config,
                      GetOutputFileFullPath(PermanentArena, &Config),
                      GetDestinationFullPath(PermanentArena, &Config));
        EndTemporaryMemory(TempMem);
    }

    printf("Backup complete.\n");
    return(0);
}
