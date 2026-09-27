#pragma once
#include <atomic>
namespace Cfg {
    inline std::atomic<bool>  esp_enabled   = true;
    inline std::atomic<bool>  esp_dead      = false;
    inline std::atomic<bool>  esp_ai        = true;
    inline std::atomic<bool>  esp_players   = true;
    inline std::atomic<bool>  esp_box       = true;
    inline std::atomic<bool>  esp_healthbar = true;
    inline std::atomic<bool>  esp_name      = true;
    inline std::atomic<bool>  esp_distance  = true;
    inline std::atomic<bool>  esp_stamina   = false;
    inline std::atomic<bool>  esp_hunger    = false;
    inline std::atomic<bool>  esp_group     = true;
    inline std::atomic<float> esp_max_dist  = 50000.f;
    inline std::atomic<bool>  aim_enabled   = false;
    inline std::atomic<bool>  aim_ai_only   = false;
    inline std::atomic<float> aim_fov       = 10.f;
    inline std::atomic<float> aim_smooth    = 6.f;
    inline std::atomic<bool>  aim_hold      = true;
    inline std::atomic<bool>  inf_stamina   = false;
    inline std::atomic<bool>  inf_hunger    = false;
    inline std::atomic<bool>  inf_thirst    = false;
}
