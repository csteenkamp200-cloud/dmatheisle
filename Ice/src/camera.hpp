#pragma once
#include "dma.hpp"
#include "esp_entity.hpp"
#include <cstdio>
#include <cmath>

struct CameraState {
    FVector pos;
    double  pitch, yaw;
    float   fov;
};

// SDK offsets (Dumper-7 confirmed)
// UWorld::OwningGameInstance        @ 0x0228
// UGameInstance::LocalPlayers data  @ 0x0038
// UPlayer::PlayerController         @ 0x0030
// APlayerController::PlayerCameraManager @ 0x0360
// APlayerCameraManager::CameraCachePrivate @ 0x1530
// FCameraCacheEntry::POV            @ 0x0010
// FMinimalViewInfo::Location        @ 0x0000
// FMinimalViewInfo::Rotation        @ 0x0018
// FMinimalViewInfo::FOV             @ 0x0030

static bool ValidPtr(uintptr_t p) {
    return p >= 0x10000ULL && (p >> 48) == 0 && (p & 0xFFFFFFFF) != 0;
}

static uintptr_t s_cam_pc = 0;

static uintptr_t ResolvePC(DMA& dma, uintptr_t gworld) {
    uintptr_t gi  = dma.Read<uintptr_t>(gworld + 0x0228);
    if (!ValidPtr(gi))  { printf("[cam] GI invalid: 0x%llX\n",  (unsigned long long)gi);  return 0; }
    uintptr_t lpd = dma.Read<uintptr_t>(gi + 0x0038);
    if (!ValidPtr(lpd)) { printf("[cam] LPD invalid: 0x%llX\n", (unsigned long long)lpd); return 0; }
    uintptr_t lp0 = dma.Read<uintptr_t>(lpd);
    if (!ValidPtr(lp0)) { printf("[cam] LP0 invalid: 0x%llX\n", (unsigned long long)lp0); return 0; }
    uintptr_t pc  = dma.Read<uintptr_t>(lp0 + 0x0030);
    if (!ValidPtr(pc))  { printf("[cam] PC invalid: 0x%llX\n",  (unsigned long long)pc);  return 0; }
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
    cam.pos        = dma.Read<FVector> (pov + 0x0000);
    FRotator rot   = dma.Read<FRotator>(pov + 0x0018);
    cam.pitch      = rot.pitch;
    cam.yaw        = rot.yaw;
    cam.fov        = dma.Read<float>   (pov + 0x0030);

    if (!std::isfinite(cam.pos.x) || cam.fov < 1.f || cam.fov > 170.f) return false;
    return true;
}
