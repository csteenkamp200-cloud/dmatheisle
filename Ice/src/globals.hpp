#pragma once
#include <cstdint>
namespace Globals {
    // fresh dump confirmed — OffsetsInfo.json 2026
    constexpr uintptr_t GWorld_Offset          = 0xB614B20; // ptr → dereference once
    constexpr uintptr_t GNames_Offset          = 0xB82A480; // FNamePool — direct addr, NO deref
    constexpr uintptr_t GObjects_Offset        = 0xB90E680; // FChunkedFixedUObjectArray — direct addr, NO deref

    constexpr uintptr_t UObject_Flags          = 0x08;
    constexpr uintptr_t UObject_Index          = 0x0C;
    constexpr uintptr_t UObject_Class          = 0x10;
    constexpr uintptr_t UObject_Name           = 0x18;
    constexpr uintptr_t UObject_Outer          = 0x20;
    constexpr uintptr_t FNamePool_Chunks       = 0x10;
    constexpr uintptr_t FNameEntry_Header      = 0x00;
    constexpr uintptr_t FNameEntry_Name        = 0x02;
    constexpr uintptr_t ULevel_Actors_Data     = 0xA0;
    constexpr uintptr_t ULevel_Actors_Count    = 0xA8;
    constexpr uintptr_t UWorld_PersistentLevel = 0x30;
}
