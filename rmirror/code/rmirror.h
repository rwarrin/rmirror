#include "miniz/miniz.h"
#include "miniz/zip.h"
#include "miniz/zip.c"

struct rmirror_config
{
    // NOTE(rick): User set variables

    wchar *RootPath;

    wchar *OutputFileName;
    b32 AppendDateToFileName;
    b32 AppendTimeToFileName;

    b32 MoveOutputFile;
    wchar *MoveToPath;

    u32 ExtensionsToIgnoreCount;
    wchar *ExtensionsToIgnore[64];

    u32 DirectoriesToIgnoreCount;
    wchar *DirectoriesToIgnore[128];

    u32 FileNamesToIgnoreCount;
    wchar *FileNamesToIgnore[128];

    u32 BackupsToKeep;

    b32 ReplaceExistingFile;

    // NOTE(rick): Application variables

    wchar *ArchiveFileName;

    b32 Valid;
};

enum file_list_flags
{
    FileListFlag_None = 0x00,
    FileListFlag_Exclude = 0x01,
};

struct file_list
{
    memory_arena *Arena;
    u32 Count;
    u32 MaxCount;
    u32 FilteredCount;
    u8 *Flags;
    wchar **Files;
};

