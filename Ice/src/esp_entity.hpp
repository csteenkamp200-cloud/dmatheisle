// esp_entity.hpp  (full replacement)
// Adds one-shot position probe — leave the console open when you first load the cheat,
// copy the CANDIDATE offset lines and paste them here if current results look wrong.
#pragma once
#include "offsets.hpp"
#include "dma.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>

struct FVector  { double x, y, z; };
struct FRotator { double pitch, yaw, roll; };

struct EntityInfo {
    uintptr_t actor_ptr;
    FVector   world_pos;
    FRotator  world_rot;
    char      steam_id[32];
    char      player_name[128];
    int32_t   char_id;
    int32_t   group_id;
    bool      is_dead, is_ai, is_god, is_baby, is_scavenger;
    float     health, max_health;
    float     stamina, max_stamina;
    float     hunger,  max_hunger;
    float     thirst,  max_thirst;
    float     oxygen, blood, max_blood, bacteria;
    float     frac_head, frac_body, frac_legs;
    float     growth, growth_amt;
};

template<typename T>
static inline T BufAt(const uint8_t* buf, size_t buf_sz, uintptr_t off) {
    T v{}; if (off + sizeof(T) <= buf_sz) memcpy(&v, buf + off, sizeof(T)); return v;
}

static std::string BufFString(DMA& dma, const uint8_t* buf, size_t buf_sz, uintptr_t field_off) {
    uintptr_t data = BufAt<uintptr_t>(buf, buf_sz, field_off);
    int32_t   len  = BufAt<int32_t> (buf, buf_sz, field_off + 8);
    if (!data || len <= 0 || len > 512) return {};
    std::vector<uint16_t> wbuf(len);
    dma.ReadBuf(data, wbuf.data(), (size_t)len * 2);
    std::string out; out.reserve(len);
    for (int i = 0; i < len; i++)
        out += (wbuf[i] < 128) ? (char)wbuf[i] : '?';
    return out;
}

// Scans 0x0C0-0x1C0 on the root component once and prints all candidates.
// Look for: large X and Y (map world coords, typically >10000 in The Isle),
//           moderate Z (terrain height, typically 0-10000).
// If the CANDIDATE at the current Off::UScene_RelativeLocation is NOT showing
// large X/Y, update that constant in offsets.hpp to the correct value.
static void ProbeEntityPosition(DMA& dma, uintptr_t actor, uintptr_t root) {
    static bool done = false;
    if (done || !root) return;
    done = true;
    printf("\n=== POSITION PROBE  actor=0x%llX  root=0x%llX ===\n",
        (unsigned long long)actor, (unsigned long long)root);
    printf("    looking for large X,Y (world) and moderate Z (terrain)\n");
    printf("    current UScene_RelativeLocation = 0x%03llX\n",
        (unsigned long long)Off::UScene_RelativeLocation);
    for (uintptr_t p = 0x0C0; p <= 0x1C0; p += 8) {
        FVector v{}; dma.ReadBuf(root + p, &v, sizeof(v));
        bool fin  = std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        bool cand = fin && (fabs(v.x) > 1000.0 || fabs(v.y) > 1000.0)
                       && fabs(v.z) < 100000.0;
        if (fin)
            printf("  [0x%03llX] (%.1f, %.1f, %.1f)%s\n",
                (unsigned long long)p, v.x, v.y, v.z,
                cand ? "  ★ CANDIDATE" : "");
    }
    printf("=== END PROBE ===\n\n");
}

bool ReadEntity(DMA& dma, uintptr_t actor, EntityInfo& out) {
    if (!actor) return false;
    out = {};
    out.actor_ptr = actor;

    static thread_local uint8_t abuf[0x2070];
    memset(abuf, 0, sizeof(abuf));
    if (!dma.ReadBuf(actor, abuf, sizeof(abuf))) return false;

    uintptr_t root = BufAt<uintptr_t>(abuf, sizeof(abuf), Off::AActor_RootComponent);
    if (!root) return false;

    // One-shot diagnostic — fires for the very first valid entity.
    // Remove this call once the correct offset is confirmed.
    ProbeEntityPosition(dma, actor, root);

    struct { FVector loc; FRotator rot; } spatial{};
    dma.ReadBuf(root + Off::UScene_RelativeLocation, &spatial, sizeof(spatial));
    out.world_pos = spatial.loc;
    out.world_rot = spatial.rot;

    out.char_id      = BufAt<int32_t>(abuf, sizeof(abuf), Off::TIChar_ID);
    out.group_id     = BufAt<int32_t>(abuf, sizeof(abuf), Off::TIChar_GroupId);
    out.is_dead      = (BufAt<uint8_t>(abuf, sizeof(abuf), Off::TIChar_bIsDead)         & 1) != 0;
    out.is_ai        = (BufAt<uint8_t>(abuf, sizeof(abuf), Off::TIChar_bIsAIControlled) & 1) != 0;
    out.is_god       = (BufAt<uint8_t>(abuf, sizeof(abuf), Off::TIChar_bIsGodMode)      & 1) != 0;
    out.is_baby      = (BufAt<uint8_t>(abuf, sizeof(abuf), Off::TIDino_bIsBaby)         & 1) != 0;
    out.is_scavenger = (BufAt<uint8_t>(abuf, sizeof(abuf), Off::TIDino_bIsScavenger)    & 1) != 0;
    out.growth       = BufAt<float>(abuf, sizeof(abuf), Off::TIDino_Growth);
    out.growth_amt   = BufAt<float>(abuf, sizeof(abuf), Off::TIDino_GrowthAmount);

    auto sid  = BufFString(dma, abuf, sizeof(abuf), Off::TIChar_SteamId);
    auto name = BufFString(dma, abuf, sizeof(abuf), Off::TIChar_PlayerName);
    snprintf(out.steam_id,    sizeof(out.steam_id),    "%s", sid.c_str());
    snprintf(out.player_name, sizeof(out.player_name), "%s", name.c_str());

    uintptr_t aset = BufAt<uintptr_t>(abuf, sizeof(abuf), Off::TIChar_AttributeSet);
    if (aset) {
        static thread_local uint8_t attrs[0x0420];
        memset(attrs, 0, sizeof(attrs));
        dma.ReadBuf(aset, attrs, sizeof(attrs));
        auto rattr = [](const uint8_t* a, uintptr_t off) -> float {
            float v{};
            uintptr_t idx = off + Off::AttrData_BaseValue;
            if (idx + sizeof(float) <= 0x0420) memcpy(&v, a + idx, sizeof(float));
            return v;
        };
        out.health      = rattr(attrs, Off::Attr_Health);
        out.max_health  = rattr(attrs, Off::Attr_MaxHealth);
        out.stamina     = rattr(attrs, Off::Attr_Stamina);
        out.max_stamina = rattr(attrs, Off::Attr_MaxStamina);
        out.hunger      = rattr(attrs, Off::Attr_Hunger);
        out.max_hunger  = rattr(attrs, Off::Attr_MaxHunger);
        out.thirst      = rattr(attrs, Off::Attr_Thirst);
        out.max_thirst  = rattr(attrs, Off::Attr_MaxThirst);
        out.oxygen      = rattr(attrs, Off::Attr_Oxygen);
        out.blood       = rattr(attrs, Off::Attr_Blood);
        out.max_blood   = rattr(attrs, Off::Attr_MaxBlood);
        out.bacteria    = rattr(attrs, Off::Attr_Bacteria);
        out.frac_head   = rattr(attrs, Off::Attr_HeadFractureHealth);
        out.frac_body   = rattr(attrs, Off::Attr_BodyFractureHealth);
        out.frac_legs   = rattr(attrs, Off::Attr_LegsFractureHealth);
    }
    return true;
}