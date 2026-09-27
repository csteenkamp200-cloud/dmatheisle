#pragma once
#include "dma.hpp"
#include "globals.hpp"
#include <cstdio>

struct BootstrapResult {
    uintptr_t module_base;
    uintptr_t gworld;    // UWorld*        — dereferenced from GWorld ptr
    uintptr_t gnames;    // FNamePool*     — direct: module_base + offset
    uintptr_t gobjects;  // FUObjectArray* — direct: module_base + offset
    bool      ok;
};

BootstrapResult Bootstrap(DMA& dma) {
    BootstrapResult r{};

    r.module_base = dma.GetModuleBase(L"TheIsleClient-Win64-Shipping.exe");
    printf("[boot] module_base = %llX\n", r.module_base);
    if (!r.module_base) return r;

    // probe read flags against MZ header before anything else
    dma.ProbeFlags(r.module_base);

    // GWorld: static UWorld** — dereference to get UWorld*
    r.gworld = dma.Read<uintptr_t>(r.module_base + Globals::GWorld_Offset);

    // GNames: FNamePool is a static OBJECT, not a pointer — use address directly
    r.gnames = r.module_base + Globals::GNames_Offset;

    // GObjects: FChunkedFixedUObjectArray is a static OBJECT — use address directly
    r.gobjects = r.module_base + Globals::GObjects_Offset;

    printf("[boot] GWorld   = %llX\n", r.gworld);
    printf("[boot] GNames   = %llX  (direct)\n", r.gnames);
    printf("[boot] GObjects = %llX  (direct)\n", r.gobjects);

    // sanity: read first chunk ptr from GNames pool (should be heap-like)
    uintptr_t names_chunk0 = dma.Read<uintptr_t>(r.gnames + Globals::FNamePool_Chunks);
    printf("[boot] GNames.chunk[0] = %llX%s\n", names_chunk0,
        names_chunk0 > 0x100000000ULL ? "  <-- looks valid" : "  <-- BAD");

    r.ok = (r.gworld && r.gnames && r.gobjects);
    return r;
}
