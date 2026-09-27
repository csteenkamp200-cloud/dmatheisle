// esp_filter.hpp  (full replacement)
#pragma once
#include "esp_entity.hpp"
#include <string>

inline bool IsCharacterClass(const std::string& cls) {
    if (cls.size() < 2) return false;
    if (cls.rfind("TI", 0) == 0) {
        // Non-character TI-prefixed classes — extend this list as new false positives appear
        static const char* blocklist[] = {
            "Ambient", "Zone", "Gore", "Nest", "Subsystem", "Manager",
            "Effect", "Projectile", "GameMode", "GameState", "PlayerState",
            "PlayerController", "HUD", "Widget", "Volume", "Trigger",
            "Light", "Decal", "Emitter", "Sound", "Fog", "Sky", "Water",
            "Landscape", "Brush", "Info", "Settings", "Spawner", "Waypoint",
            "Item", "Pickup", "Prop", "Structure", "Gate", "Door", "Fence",
        };
        for (auto b : blocklist)
            if (cls.find(b) != std::string::npos) return false;
        return true;
    }
    return false;
}

// Sanity check on a fully-read entity before rendering or aiming at it.
// Growth outside 0.0-1.0 = garbage read from a misclassified actor.
// Zero max_health = attribute set didn't resolve = also garbage.
inline bool EntitySane(const EntityInfo& e) {
    if (e.growth    <  0.f  || e.growth    > 1.05f) return false; // 1.05 for float imprecision
    if (e.max_health < 0.f  || e.max_health > 99999.f) return false;
    // Position sanity — The Isle map coords are finite and not absurdly large
    if (e.world_pos.x == 0.0 && e.world_pos.y == 0.0) return false; // null read
    return true;
}

inline std::string ESPLabel(const EntityInfo& e) {
    char buf[256];
    if (e.player_name[0])
        snprintf(buf, sizeof(buf), "%s | HP %.0f/%.0f | G%.2f | Grp%d",
            e.player_name, e.health, e.max_health, e.growth, e.group_id);
    else
        snprintf(buf, sizeof(buf), "AI | HP %.0f/%.0f | G%.2f",
            e.health, e.max_health, e.growth);
    return buf;
}