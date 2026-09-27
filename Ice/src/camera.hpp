#pragma once
#include "dma.hpp"
#include "esp_entity.hpp"
#include <cstdio>
#include <cmath>
#include <cstring>

struct CameraState {
    FVector pos;
    double  pitch, yaw;
    float   fov;
};

static bool ValidPtr(uintptr_t p) {
    return p >= 0x10000ULL && (p >> 48) == 0 && (p & 0xFFFFFFFF) != 0;
}

static uintptr_t s_cam_pc     = 0;
static bool      s_layout_set = false;
static bool      s_float_rot  = false; // true = FRotator is 3×float(12B), false = 3×double(24B)

static uintptr_t ResolvePC(DMA& dma, uintptr_t gworld) {
    uintptr_t gi  = dma.Read<uintptr_t>(gworld + 0x0228);
    if (!ValidPtr(gi))  { printf("[cam] GI  bad: 0x%llX\n", (unsigned long long)gi);  return 0; }
    uintptr_t lpd = dma.Read<uintptr_t>(gi + 0x0038);
    if (!ValidPtr(lpd)) { printf("[cam] LPD bad: 0x%llX\n", (unsigned long long)lpd); return 0; }
    uintptr_t lp0 = dma.Read<uintptr_t>(lpd);
    if (!ValidPtr(lp0)) { printf("[cam] LP0 bad: 0x%llX\n", (unsigned long long)lp0); return 0; }
    uintptr_t pc  = dma.Read<uintptr_t>(lp0 + 0x0030);
    if (!ValidPtr(pc))  { printf("[cam] PC  bad: 0x%llX\n", (unsigned long long)pc);  return 0; }
    printf("[cam] resolved  GI=0x%llX  LP0=0x%llX  PC=0x%llX\n",
        (unsigned long long)gi, (unsigned long long)lp0, (unsigned long long)pc);
    return pc;
}

bool ReadCamera(DMA& dma, uintptr_t gworld, CameraState& cam) {
    if (!s_cam_pc || !ValidPtr(s_cam_pc))
        s_cam_pc = ResolvePC(dma, gworld);
    if (!s_cam_pc) return false;

    uintptr_t pcm = dma.Read<uintptr_t>(s_cam_pc + 0x0360);
    if (!ValidPtr(pcm)) { s_cam_pc = 0; return false; }

    uintptr_t pov = pcm + 0x1530 + 0x0010;

    // Single 64B bulk read covers both possible layouts:
    //   float  FRotator: FVector(24) + FRotator_f(12) + FOV(4)  — FOV @ 0x24
    //   double FRotator: FVector(24) + FRotator_d(24) + FOV(4)  — FOV @ 0x30
    uint8_t buf[64] = {};
    if (!dma.ReadBuf(pov, buf, sizeof(buf))) return false;

    // Auto-detect layout once using FOV sanity (valid game range 50-150)
    // Retries until camera is live and FOV is readable
    if (!s_layout_set) {
        float fov24 = 0.f, fov30 = 0.f;
        memcpy(&fov24, buf + 0x24, 4);
        memcpy(&fov30, buf + 0x30, 4);
        bool v24 = (fov24 >= 50.f && fov24 <= 150.f);
        bool v30 = (fov30 >= 50.f && fov30 <= 150.f);
        if (v24 || v30) {
            s_float_rot  = (v24 && !v30);
            s_layout_set = true;
            printf("[cam] layout probe: FOV@0x24=%.2f  FOV@0x30=%.2f  -> %s FRotator\n",
                fov24, fov30, s_float_rot ? "float(12B)" : "double(24B)");
        }
        // Not locked in yet — still try to fill cam from what we have
    }

    // Location — always FVector (3×double, 24B) in UE5 LWC
    memcpy(&cam.pos, buf + 0x00, sizeof(FVector));

    if (s_float_rot) {
        float fp = 0.f, fy = 0.f, ff = 0.f;
        memcpy(&fp, buf + 0x18, 4);  // Pitch
        memcpy(&fy, buf + 0x1C, 4);  // Yaw
        memcpy(&ff, buf + 0x24, 4);  // FOV
        cam.pitch = (double)fp;
        cam.yaw   = (double)fy;
        cam.fov   = ff;
    } else {
        double dp = 0.0, dy = 0.0;
        float  df = 0.f;
        memcpy(&dp, buf + 0x18, 8);  // Pitch
        memcpy(&dy, buf + 0x20, 8);  // Yaw
        memcpy(&df, buf + 0x30, 4);  // FOV
        cam.pitch = dp;
        cam.yaw   = dy;
        cam.fov   = df;
    }

    if (!std::isfinite(cam.pos.x) || cam.fov < 1.f || cam.fov > 170.f) return false;

    // Pitch debug — printed every ~120 frames (~1s at 8ms tick)
    // Watch this while looking up and down — if pitch stays near 0.0 the
    // CameraCachePrivate offset (0x1530) is wrong; send APlayerCameraManager.hpp
    static int s_dbg = 0;
    if (++s_dbg >= 120) {
        s_dbg = 0;
        printf("[cam] pitch=%.2f  yaw=%.2f  fov=%.1f  pos=(%.0f,%.0f,%.0f)\n",
            cam.pitch, cam.yaw, cam.fov,
            cam.pos.x, cam.pos.y, cam.pos.z);
    }

    return true;
}