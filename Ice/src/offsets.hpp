#pragma once
#include <cstdint>
namespace Off {
    constexpr uintptr_t UWorld_PersistentLevel  = 0x030;
    constexpr uintptr_t ULevel_Actors_Data      = 0x0A0;
    constexpr uintptr_t ULevel_Actors_Count     = 0x0A8;
    constexpr uintptr_t AActor_RootComponent    = 0x1B8;
    constexpr uintptr_t UScene_RelativeLocation = 0x140;
    constexpr uintptr_t UScene_RelativeRotation = 0x158;
    constexpr uintptr_t TIChar_ID               = 0x08A4;
    constexpr uintptr_t TIChar_SteamId          = 0x0AC0;
    constexpr uintptr_t TIChar_ASC              = 0x0AE0;
    constexpr uintptr_t TIChar_LowHealth        = 0x0D6C;
    constexpr uintptr_t TIChar_AttributeSet     = 0x13A0;
    constexpr uintptr_t TIChar_PlayerController = 0x1618;
    constexpr uintptr_t TIChar_bIsGodMode       = 0x18FA;
    constexpr uintptr_t TIChar_bIsDead          = 0x1958;
    constexpr uintptr_t TIChar_bIsAIControlled  = 0x1AA8;
    constexpr uintptr_t TIChar_PlayerName       = 0x1AE8;
    constexpr uintptr_t TIChar_GroupId          = 0x1B10;
    constexpr uintptr_t TIDino_FatherId         = 0x1BB0;
    constexpr uintptr_t TIDino_MotherId         = 0x1BB4;
    constexpr uintptr_t TIDino_DinosaurAssets   = 0x1BD8;
    constexpr uintptr_t TIDino_Growth           = 0x1E68;
    constexpr uintptr_t TIDino_GrowthAmount     = 0x1E6C;
    constexpr uintptr_t TIDino_bIsScavenger     = 0x2061;
    constexpr uintptr_t TIDino_bIsBaby          = 0x2062;
    constexpr uintptr_t Attr_Health             = 0x0030;
    constexpr uintptr_t Attr_MaxHealth          = 0x0040;
    constexpr uintptr_t Attr_Stamina            = 0x0050;
    constexpr uintptr_t Attr_MaxStamina         = 0x0060;
    constexpr uintptr_t Attr_Hunger             = 0x0070;
    constexpr uintptr_t Attr_MaxHunger          = 0x0080;
    constexpr uintptr_t Attr_Thirst             = 0x0090;
    constexpr uintptr_t Attr_MaxThirst          = 0x00A0;
    constexpr uintptr_t Attr_Oxygen             = 0x0280;
    constexpr uintptr_t Attr_Blood              = 0x0330;
    constexpr uintptr_t Attr_MaxBlood           = 0x0340;
    constexpr uintptr_t Attr_Bacteria           = 0x0410;
    constexpr uintptr_t Attr_HeadFractureHealth = 0x03B0;
    constexpr uintptr_t Attr_BodyFractureHealth = 0x03D0;
    constexpr uintptr_t Attr_LegsFractureHealth = 0x03F0;
    constexpr uintptr_t AttrData_BaseValue      = 0x08;
    constexpr uintptr_t AttrData_CurrentValue   = 0x0C;
    constexpr uintptr_t ASC_SpawnedAttributes   = 0x10A8;
}
