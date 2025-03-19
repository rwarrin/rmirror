#ifndef RMIRROR_PLATFORM

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef size_t mem_size;
typedef intptr_t smm;
typedef uintptr_t umm;
typedef int32_t b32;
typedef wchar_t wchar;

#define ArrayCount(Array) (sizeof((Array)) / sizeof((Array)[0]))

#define Assert(Condition) do { if(!(Condition)) { *(s32 *)0 = 0; } } while(0)

#define global static
#define local_persist static
#define internal static

inline u32
BitscanReverse(u32 Value, s32 ByteCount)
{
    u32 Result = 0;
    s32 BitCount = ByteCount*8;
    for(s32 Index = BitCount - 1; Index >= 0; --Index)
    {
        if(Value & (1 << Index))
        {
            Result = Index;
            break;
        }
    }

    return(Result);
}

inline b32
StringsMatch(char *A, char *B)
{
    b32 Result = false;

    if(A && B)
    {
        for(;;)
        {
            if(*A == 0)
            {
                Result = true;
                break;
            }

            if(*A != *B)
            {
                break;
            }

            ++A;
            ++B;
        }
    }

    return(Result);
}

inline b32
StringsMatchN(char *A, char *B, u32 Length)
{
    b32 Result = false;

    if(A && B)
    {
        Result = true;
        while(Length--)
        {
            if(*A != *B)
            {
                Result = false;
                break;
            }

            ++A;
            ++B;
        }
    }

    return(Result);
}

inline b32
WStringsMatch(wchar *A, wchar *B)
{
    b32 Result = false;

    if(A && B)
    {
        for(;;)
        {
            if(*A == L'\0')
            {
                Result = true;
                break;
            }

            if(*A != *B)
            {
                break;
            }

            ++A;
            ++B;
        }

    }

    return(Result);
}

inline u32
WStringLength(wchar *Str)
{
    u32 Result = 0;

    while(*Str++)
    {
        ++Result;
    }

    return(Result);
}

inline u32
StringLength(char *Str)
{
    u32 Result = 0;

    while(*Str++)
    {
        ++Result;
    }

    return(Result);
}

inline b32
WStringFind(wchar *Haystack, wchar *Needle)
{
    b32 Result = false;

    if(Haystack && Needle)
    {
        for(wchar *HaystackAt = Haystack; !Result && (*HaystackAt != L'\0'); ++HaystackAt)
        {
            if(*HaystackAt == *Needle)
            {
                for(wchar *HaystackScan = HaystackAt,
                    *NeedleScan = Needle;
                    HaystackScan != L'\0';
                    ++HaystackScan, ++NeedleScan)
                {
                    if(*NeedleScan == L'\0')
                    {
                        Result = true;
                        break;
                    }

                    if(*NeedleScan != *HaystackScan)
                    {
                        break;
                    }
                }
            }
        }
    }

    return(Result);
}

inline void *
Copy(mem_size Size, void *SourceInit, void *DestInit)
{
    u8 *Source = (u8 *)SourceInit;
    u8 *Dest = (u8 *)DestInit;
    while(Size--)
    {
        *Dest++ = *Source++;
    }

    return(DestInit);
}

struct arena_push_params
{
    u8 Alignment;
};

inline arena_push_params
DefaultPushParams()
{
    arena_push_params Result = {0};
    Result.Alignment = 4;
    return(Result);
}

struct memory_arena
{
    u8 *Base;
    mem_size Used;
    mem_size Size;
    s8 TempCount;
};

#define PushStruct(Arena, type, ...) (type *)ArenaPush_(Arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(Arena, type, count, ...) (type *)ArenaPush_(Arena, sizeof(type)*(count), ## __VA_ARGS__)
#define PushSize(Arena, size, ...) ArenaPush_(Arena, size, ## __VA_ARGS__)

inline void *
ArenaPush_(memory_arena *Arena, mem_size Size, arena_push_params Params = DefaultPushParams())
{
    umm InitialPtr = (umm)(Arena->Base + Arena->Used);
    mem_size AlignmentOffset = Params.Alignment - (InitialPtr & (Params.Alignment - 1));
    mem_size TotalSize = Size + AlignmentOffset;

    Assert((Arena->Used + TotalSize) < Arena->Size);

    void *Result = (void *)((u8 *)InitialPtr + AlignmentOffset);
    Arena->Used += TotalSize;

    return(Result);
}
inline char *
PushString(memory_arena *Arena, char *Source)
{
    u32 Length = StringLength(Source) + 1;
    char *Dest = PushArray(Arena, char, Length);
    Copy(Length - 1, Source, Dest);
    Dest[Length] = 0;
    return(Dest);
}

inline wchar *
PushWString(memory_arena *Arena, wchar *Source)
{
    u32 Length = WStringLength(Source) + 1;
    wchar *Dest = PushArray(Arena, wchar, Length);
    Copy((Length-1)*sizeof(*Source), Source, Dest);
    Dest[Length] = L'\0';
    return(Dest);
}

#define Kilobytes(size) ((size)*1024LL)
#define Megabytes(size) (Kilobytes(size)*1024LL)
#define Gigabytes(size) (Megabytes(size)*1024LL)
#define Terabytes(size) (Gigabytes(size)*1024LL)

enum arena_creation_flags
{
    ArenaCreation_RoundToNextPowerOfTwo = 0x01,
};
internal memory_arena *
CreateArena(mem_size Size, mem_size BaseLocation = 0, u32 CreationFlags = 0)
{
    if(CreationFlags & ArenaCreation_RoundToNextPowerOfTwo)
    {
        Size = 1 << (BitscanReverse(Size, sizeof(Size)) + 1);
    }
    mem_size TotalSize = Size + sizeof(memory_arena);

    u8 *Memory = (u8 *)VirtualAlloc((void *)BaseLocation, TotalSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    Assert(Memory);

    memory_arena *Result = (memory_arena *)Memory;
    Result->Base = (u8 *)(Result + 1);
    Result->Used = 0;
    Result->Size = Size;

    return(Result);
}

internal void
DestroyArena(memory_arena *Arena)
{
    Assert(Arena);

    Arena->Base = 0;
    Arena->Used = 0;
    Arena->Size = 0;
    VirtualFree(Arena, 0, MEM_RELEASE);
}

struct temp_memory
{
    memory_arena *Arena;
    mem_size Mark;
};

internal temp_memory
BeginTemporaryMemory(memory_arena *Arena)
{
    temp_memory Result = {0};
    Result.Arena = Arena;
    Result.Mark = Arena->Used;
    ++Arena->TempCount;
    return(Result);
}

internal void
EndTemporaryMemory(temp_memory Temp)
{
    memory_arena *Arena = Temp.Arena;
    Arena->Used = Temp.Mark;
    --Arena->TempCount;
    Assert(Arena->TempCount >= 0);
}

struct scratch_arena
{
    scratch_arena(mem_size Size)
    {
        mem_size ArenaSize = ((Size) ? Size : Kilobytes(64));
        Arena = CreateArena(ArenaSize);
    }

    ~scratch_arena()
    {
        DestroyArena(Arena);
    }

    memory_arena *Arena;
};

#define make_scratch_name_(A, B) A##B
#define make_scratch_name(A, B) make_scratch_name_(A, B)
#define make_scratch(Var, size) 0;\
    scratch_arena make_scratch_name(scratch_, __LINE__)(size); \
    Var = make_scratch_name(scratch_, __LINE__).Arena

#define ZeroStruct(Instance) ZeroSize(&(Instance), sizeof(Instance))
#define ZeroArray(Pointer, Count) ZeroSize((Pointer), sizeof((Pointer)[0])*(Count))
inline void
ZeroSize(void *Ptr, mem_size Size)
{
    u8 *Byte = (u8 *)Ptr;
    while(Size--)
    {
        *Byte++ = 0;
    }
}

inline wchar *
CStrToWCStr(memory_arena *Arena, char *CStr)
{
    u32 Length = StringLength(CStr);
    wchar *Result = PushArray(Arena, wchar, Length+1);
    for(u32 Index = 0; Index < Length; ++Index)
    {
        Result[Index] = btowc(CStr[Index]);
    }
    Result[Length] = 0;

    return(Result);
}

#define GET_TIME_T_FROM_FILE(name) b32 (name)(FILE *File, time_t *TimeT)
typedef GET_TIME_T_FROM_FILE(get_time_t_from_file);
extern get_time_t_from_file *GetTimeTFromFile;

#define RMIRROR_PLATFORM
#endif
