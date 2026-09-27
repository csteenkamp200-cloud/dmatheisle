// language: C++, file: aimbot.hpp, target: Windows 11, MSVC
#pragma once
#include "math.hpp"
#include "esp_entity.hpp"
#include "esp_filter.hpp"
#include "camera.hpp"
#include "cheat.hpp"
#include <vector>
#include <windows.h>

struct AimResult { bool valid; double delta_pitch, delta_yaw; uintptr_t target_ptr; };

AimResult RunAimbot(const std::vector<EntityInfo>& entities, const CameraState& cam) {
    AimResult res{};
    if (!Cfg::aim_enabled) return res;
    if (Cfg::aim_hold && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) return res;
    Angle current{ cam.pitch, cam.yaw };
    double best = 1e18;
    const EntityInfo* tgt = nullptr;
    for (auto& e : entities) {
        if (!EntitySane(e))                continue;
        if (e.is_dead)                     continue;
        if (Cfg::aim_ai_only && !e.is_ai)  continue;
        if (!Cfg::aim_ai_only && e.is_ai)  continue;
        double hz = e.growth > 0.1f ? 80.0 + e.growth*120.0 : 90.0;
        FVector hp{ e.world_pos.x, e.world_pos.y, e.world_pos.z + hz };
        Angle to = CalcAngle(cam.pos, hp);
        double ad = AngleDist(current, to);
        if (ad > (double)Cfg::aim_fov) continue;
        if (ad < best) { best = ad; tgt = &e; }
    }
    if (!tgt) return res;
    double hz = tgt->growth > 0.1f ? 80.0 + tgt->growth*120.0 : 90.0;
    FVector hp{ tgt->world_pos.x, tgt->world_pos.y, tgt->world_pos.z + hz };
    Angle to = CalcAngle(cam.pos, hp);
    double sm = (double)Cfg::aim_smooth.load();
    res.valid       = true;
    res.delta_pitch = AngleDiff(to.pitch, cam.pitch) / sm;
    res.delta_yaw   = AngleDiff(to.yaw,   cam.yaw)   / sm;
    res.target_ptr  = tgt->actor_ptr;
    return res;
}