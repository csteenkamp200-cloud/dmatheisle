#pragma once
#include "esp_entity.hpp"
#include <string>

inline bool IsCharacterClass(const std::string& cls) {
    if (cls.size() < 2) return false;
    if (cls.rfind("TI", 0) == 0) {
        if (cls.find("Ambient")    != std::string::npos) return false;
        if (cls.find("Zone")       != std::string::npos) return false;
        if (cls.find("Gore")       != std::string::npos) return false;
        if (cls.find("Nest")       != std::string::npos) return false;
        if (cls.find("Subsystem")  != std::string::npos) return false;
        if (cls.find("Manager")    != std::string::npos) return false;
        if (cls.find("Effect")     != std::string::npos) return false;
        if (cls.find("Projectile") != std::string::npos) return false;
        return true;
    }
    return false;
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
