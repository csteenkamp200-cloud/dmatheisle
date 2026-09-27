#pragma once
#include "offsets.hpp"
#include "dma.hpp"
#include <string>
#include <vector>
#include <cstdio>

// UE5 LWC: FVector = 3x double (24 bytes), FRotator = 3x double (24 bytes)
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

static std::string ReadFString(DMA& dma, uintptr_t addr) {
    uintptr_t data = dma.Read<uintptr_t>(addr);
    int32_t   len  = dma.Read<int32_t>  (addr + 8);
    if (!data || len <= 0 || len > 512) return {};
    std::vector<uint16_t> buf(len);
    dma.ReadBuf(data, buf.data(), len * 2);
    std::string out; out.reserve(len);
    for (int i = 0; i < len; i++)
        out += (buf[i] < 128) ? (char)buf[i] : '?';
    return out;
}

static inline float ReadAttr(DMA& dma, uintptr_t aset, uintptr_t off) {
    return dma.Read<float>(aset + off + Off::AttrData_BaseValue);
}

bool ReadEntity(DMA& dma, uintptr_t actor, EntityInfo& out) {
    if (!actor) return false;
    out = {};
    out.actor_ptr = actor;
    uintptr_t root = dma.Read<uintptr_t>(actor + Off::AActor_RootComponent);
    if (!root) return false;
    // FVector/FRotator are 24 bytes (3x double) — read as struct directly
    out.world_pos = dma.Read<FVector> (root + Off::UScene_RelativeLocation);
    out.world_rot = dma.Read<FRotator>(root + Off::UScene_RelativeRotation);
    out.char_id   = dma.Read<int32_t>(actor + Off::TIChar_ID);
    out.group_id  = dma.Read<int32_t>(actor + Off::TIChar_GroupId);
    auto sid  = ReadFString(dma, actor + Off::TIChar_SteamId);
    auto name = ReadFString(dma, actor + Off::TIChar_PlayerName);
    snprintf(out.steam_id,    sizeof(out.steam_id),    "%s", sid.c_str());
    snprintf(out.player_name, sizeof(out.player_name), "%s", name.c_str());
    out.is_dead      = (dma.Read<uint8_t>(actor + Off::TIChar_bIsDead)         & 1) != 0;
    out.is_ai        = (dma.Read<uint8_t>(actor + Off::TIChar_bIsAIControlled) & 1) != 0;
    out.is_god       = (dma.Read<uint8_t>(actor + Off::TIChar_bIsGodMode)      & 1) != 0;
    out.is_baby      = (dma.Read<uint8_t>(actor + Off::TIDino_bIsBaby)         & 1) != 0;
    out.is_scavenger = (dma.Read<uint8_t>(actor + Off::TIDino_bIsScavenger)    & 1) != 0;
    uintptr_t aset   = dma.Read<uintptr_t>(actor + Off::TIChar_AttributeSet);
    if (aset) {
        out.health      = ReadAttr(dma, aset, Off::Attr_Health);
        out.max_health  = ReadAttr(dma, aset, Off::Attr_MaxHealth);
        out.stamina     = ReadAttr(dma, aset, Off::Attr_Stamina);
        out.max_stamina = ReadAttr(dma, aset, Off::Attr_MaxStamina);
        out.hunger      = ReadAttr(dma, aset, Off::Attr_Hunger);
        out.max_hunger  = ReadAttr(dma, aset, Off::Attr_MaxHunger);
        out.thirst      = ReadAttr(dma, aset, Off::Attr_Thirst);
        out.max_thirst  = ReadAttr(dma, aset, Off::Attr_MaxThirst);
        out.oxygen      = ReadAttr(dma, aset, Off::Attr_Oxygen);
        out.blood       = ReadAttr(dma, aset, Off::Attr_Blood);
        out.max_blood   = ReadAttr(dma, aset, Off::Attr_MaxBlood);
        out.bacteria    = ReadAttr(dma, aset, Off::Attr_Bacteria);
        out.frac_head   = ReadAttr(dma, aset, Off::Attr_HeadFractureHealth);
        out.frac_body   = ReadAttr(dma, aset, Off::Attr_BodyFractureHealth);
        out.frac_legs   = ReadAttr(dma, aset, Off::Attr_LegsFractureHealth);
    }
    out.growth     = dma.Read<float>(actor + Off::TIDino_Growth);
    out.growth_amt = dma.Read<float>(actor + Off::TIDino_GrowthAmount);
    return true;
}
