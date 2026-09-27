#pragma once
#include "dma.hpp"
#include "offsets.hpp"
#include "cheat.hpp"

static void WriteAttr(DMA& dma, uintptr_t aset, uintptr_t off, float val) {
    dma.Write<float>(aset + off + Off::AttrData_BaseValue,    val);
    dma.Write<float>(aset + off + Off::AttrData_CurrentValue, val);
}

void ApplyMisc(DMA& dma, uintptr_t local_actor) {
    if (!local_actor) return;
    uintptr_t aset = dma.Read<uintptr_t>(local_actor + Off::TIChar_AttributeSet);
    if (!aset) return;
    if (Cfg::inf_stamina) {
        float m = dma.Read<float>(aset + Off::Attr_MaxStamina + Off::AttrData_BaseValue);
        WriteAttr(dma, aset, Off::Attr_Stamina, m);
    }
    if (Cfg::inf_hunger) {
        float m = dma.Read<float>(aset + Off::Attr_MaxHunger + Off::AttrData_BaseValue);
        WriteAttr(dma, aset, Off::Attr_Hunger, m);
    }
    if (Cfg::inf_thirst) {
        float m = dma.Read<float>(aset + Off::Attr_MaxThirst + Off::AttrData_BaseValue);
        WriteAttr(dma, aset, Off::Attr_Thirst, m);
    }
}
