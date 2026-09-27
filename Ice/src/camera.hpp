#pragma once
#include "dma.hpp"
#include "esp_entity.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>

struct CameraState {
    FVector pos;
    double  pitch, yaw;
    float   fov;
};

namespace CamOff {
    inline int       APC_CameraManager = 0x360;
    inline uintptr_t ACM_CacheOff      = 0x1530;
    inline bool      ready             = false;
    constexpr int    CacheEntry_POV    = 0x10;
}

// ── helper: is pointer plausible game heap address ────────────
static bool ValidGamePtr(uintptr_t p) {
    // top 16 bits must be 0 (user-mode), not null, not suspiciously aligned
    return p >= 0x10000ULL && (p >> 48) == 0 && (p & 0xFFFFFFFF) != 0;
}

// ── GWorld → GameInstance → LocalPlayers[0] → PlayerController ─
uintptr_t GetLocalPC(DMA& dma, uintptr_t gworld) {
    // try a few GameInstance offsets — UE5 varies slightly per build
    static const uintptr_t gi_offs[] = { 0x1B8, 0x1C0, 0x1D0, 0x1E0 };
    for (auto go : gi_offs) {
        uintptr_t gi = dma.Read<uintptr_t>(gworld + go);
        if (!ValidGamePtr(gi)) continue;

        // UGameInstance::LocalPlayers TArray: data ptr @ 0x38, count @ 0x40
        uintptr_t lp_data = dma.Read<uintptr_t>(gi + 0x38);
        int32_t   lp_cnt  = dma.Read<int32_t>  (gi + 0x40);
        if (!ValidGamePtr(lp_data) || lp_cnt <= 0 || lp_cnt > 8) continue;

        uintptr_t lp0 = dma.Read<uintptr_t>(lp_data);   // LocalPlayers[0]
        if (!ValidGamePtr(lp0)) continue;

        // ULocalPlayer::PlayerController @ 0x30
        uintptr_t pc = dma.Read<uintptr_t>(lp0 + 0x30);
        if (ValidGamePtr(pc)) {
            printf("[cam] found PC via world+0x%llX  gi=0x%llX  pc=0x%llX\n",
                (unsigned long long)go,
                (unsigned long long)gi,
                (unsigned long long)pc);
            return pc;
        }
    }
    return 0;
}

// ── scan PCM object for a FOV/pos/rot cluster ─────────────────
static bool TryScanCache(DMA& dma, uintptr_t mgr, int mgr_off) {
    // scan 0x400–0x4000, step 8
    constexpr size_t START = 0x400;
    constexpr size_t END   = 0x4000;
    constexpr size_t SZ    = END - START;
    uint8_t buf[SZ];
    if (!dma.ReadBuf(mgr + START, buf, SZ)) {
        printf("[cam-scan] ReadBuf failed at mgr=0x%llX\n", (unsigned long long)mgr);
        return false;
    }
    for (size_t off = 0; off + 0x40 <= SZ; off += 8) {
        size_t p = off + CamOff::CacheEntry_POV;
        if (p + 0x34 > SZ) break;

        float fov;
        memcpy(&fov, buf + p + 0x30, 4);
        if (fov < 50.f || fov > 120.f) continue;

        double x, y, z, pitch, yaw;
        memcpy(&x,     buf + p + 0x00, 8);
        memcpy(&y,     buf + p + 0x08, 8);
        memcpy(&z,     buf + p + 0x10, 8);

        // world coords in UE cm — map is ~600km, height rarely >300km
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
        if (x < -6e7 || x > 6e7 || y < -6e7 || y > 6e7)  continue;
        if (z < -1e5 || z > 3e5) continue;

        memcpy(&pitch, buf + p + 0x18, 8);
        memcpy(&yaw,   buf + p + 0x20, 8);
        if (!std::isfinite(pitch) || !std::isfinite(yaw))  continue;
        if (pitch < -91.0 || pitch > 91.0) continue;
        if (yaw  < -361.0 || yaw  > 361.0) continue;

        uintptr_t cache_off = START + off;
        CamOff::ACM_CacheOff      = cache_off;
        CamOff::APC_CameraManager = mgr_off;
        printf("[cam-scan] HIT  PCM=pc+0x%X  CacheOff=0x%llX\n",
            mgr_off, (unsigned long long)cache_off);
        printf("[cam-scan]      pos=(%.0f,%.0f,%.0f)  pitch=%.1f  yaw=%.1f  fov=%.1f\n",
            x, y, z, pitch, yaw, fov);
        return true;
    }
    return false;
}

// ── one-shot probe: called with a VALID pc ────────────────────
bool ProbeCamera(DMA& dma, uintptr_t pc) {
    printf("[cam-probe] scanning pc=0x%llX\n", (unsigned long long)pc);

    // collect candidate PCM pointers from pc in range [0x280, 0x600]
    struct Cand { uintptr_t ptr; int off; };
    Cand cands[128]; int nc = 0;

    for (int alt = 0x280; alt <= 0x600 && nc < 128; alt += 8) {
        uintptr_t m = dma.Read<uintptr_t>(pc + alt);
        if (!ValidGamePtr(m)) continue;
        bool dup = false;
        for (int i = 0; i < nc; i++) if (cands[i].ptr == m) { dup = true; break; }
        if (!dup) cands[nc++] = { m, alt };
    }
    printf("[cam-probe] %d PCM candidates\n", nc);

    for (int i = 0; i < nc; i++) {
        printf("[cam-probe] trying pc+0x%03X=0x%llX\n",
            cands[i].off, (unsigned long long)cands[i].ptr);
        if (TryScanCache(dma, cands[i].ptr, cands[i].off)) {
            CamOff::ready = true;
            return true;
        }
    }
    puts("[cam-probe] FAIL — no FOV cluster found; are you fully spawned as a dino?");
    return false;
}

bool ReadCamera(DMA& dma, uintptr_t pc, CameraState& cam) {
    if (!CamOff::ready || !pc) return false;
    uintptr_t mgr = dma.Read<uintptr_t>(pc + CamOff::APC_CameraManager);
    if (!ValidGamePtr(mgr)) return false;
    uintptr_t pov  = mgr + CamOff::ACM_CacheOff + CamOff::CacheEntry_POV;
    cam.pos        = dma.Read<FVector> (pov + 0x00);
    FRotator rot   = dma.Read<FRotator>(pov + 0x18);
    cam.pitch      = rot.pitch;
    cam.yaw        = rot.yaw;
    cam.fov        = dma.Read<float>   (pov + 0x30);
    return cam.fov > 0.f;
}
