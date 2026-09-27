#include <windows.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <cstdio>
#include "dma.hpp"
#include "globals.hpp"
#include "fname.hpp"
#include "bootstrap.hpp"
#include "offsets.hpp"
#include "esp_entity.hpp"
#include "esp_filter.hpp"
#include "camera.hpp"
#include "math.hpp"
#include "aimbot.hpp"
#include "overlay.hpp"
#include "misc.hpp"
#include "cheat.hpp"

struct SharedState {
    std::vector<EntityInfo> entities;
    CameraState             cam{};
    uintptr_t               local_actor=0, aim_target=0;
    std::mutex              mtx;
};

static BootstrapResult g_br;

void EntityThread(DMA& dma, BootstrapResult& br, SharedState& s) {
    int log_tick = 0;
    while(true){
        uintptr_t gworld  = dma.Read<uintptr_t>(br.module_base + Globals::GWorld_Offset);
        uintptr_t level   = dma.Read<uintptr_t>(gworld + Globals::UWorld_PersistentLevel);
        uintptr_t aptr    = dma.Read<uintptr_t>(level + Globals::ULevel_Actors_Data);
        int32_t   count   = dma.Read<int32_t>  (level + Globals::ULevel_Actors_Count);

        if(!aptr || count<=0 || count>8192){
            if(log_tick++ % 30 == 0)
                printf("[ent] world=%llX level=%llX aptr=%llX count=%d\n",
                    gworld, level, aptr, count);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        std::vector<uintptr_t> actors(count);
        dma.ReadBuf(aptr, actors.data(), (size_t)count * 8);

        std::vector<EntityInfo> frame; frame.reserve(128);
        uintptr_t local = 0;

        for(auto actor : actors){
            if(!actor) continue;
            std::string cls = GetObjectClassName(dma, br.gnames, actor);
            if(!IsCharacterClass(cls)) continue;
            EntityInfo e{};
            if(!ReadEntity(dma, actor, e)) continue;
            if(!e.is_ai){
                uintptr_t pc = dma.Read<uintptr_t>(actor + Off::TIChar_PlayerController);
                if(pc) local = actor;
            }
            frame.push_back(e);
        }

        CameraState cam{};
        if(local){
            // derive PC from world — bypasses bad actor offset
            static uintptr_t s_pc = 0;
            static bool s_probed = false;
            if(!s_pc || !s_probed) {
                uintptr_t wp = GetLocalPC(dma, gworld);
                if(wp) {
                    s_pc = wp;
                    if(!s_probed) {
                        printf("[ET] probing with world-PC=0x%llX\n",(unsigned long long)wp);
                        s_probed = ProbeCamera(dma, wp);
                    }
                }
            }
            if(s_pc) ReadCamera(dma, s_pc, cam);
        }

        // log every ~2 seconds
        if(log_tick++ % 60 == 0){
            printf("\n[tick] actors=%d  entities=%zu  local=%llX\n",
                count, frame.size(), local);
            printf("[cam]  pos=(%.0f, %.0f, %.0f)  yaw=%.1f  pitch=%.1f  fov=%.1f\n",
                cam.pos.x, cam.pos.y, cam.pos.z,
                cam.yaw, cam.pitch, cam.fov);
            for(size_t i = 0; i < frame.size() && i < 8; i++){
                auto& e = frame[i];
                printf("[ent%zu] %s  pos=(%.0f,%.0f,%.0f)  hp=%.0f/%.0f  ai=%d  dead=%d\n",
                    i,
                    e.player_name[0] ? e.player_name : (e.steam_id[0] ? e.steam_id : "?"),
                    e.world_pos.x, e.world_pos.y, e.world_pos.z,
                    e.health, e.max_health,
                    (int)e.is_ai, (int)e.is_dead);

                // W2S test against screen center
                Vec2 sc{};
                float fov = cam.fov > 0 ? cam.fov : 90.f;
                bool vis = WorldToScreen(e.world_pos, cam.pos,
                    cam.yaw, cam.pitch, fov, 1920, 1080, sc);
                printf("       W2S: visible=%d  screen=(%.0f, %.0f)\n",
                    (int)vis, sc.x, sc.y);
            }
        }

        { std::lock_guard<std::mutex> lk(s.mtx);
          s.entities = std::move(frame);
          s.cam = cam;
          s.local_actor = local; }

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}

void AimThread(DMA& dma, SharedState& s){
    while(true){
        std::vector<EntityInfo> ents; CameraState cam; uintptr_t local;
        { std::lock_guard<std::mutex> lk(s.mtx);
          ents=s.entities; cam=s.cam; local=s.local_actor; }
        AimResult aim = RunAimbot(ents, cam);
        if(aim.valid){
            POINT cur; GetCursorPos(&cur);
            SetCursorPos(cur.x+(int)(aim.delta_yaw*10.f),
                         cur.y-(int)(aim.delta_pitch*10.f));
            std::lock_guard<std::mutex> lk(s.mtx);
            s.aim_target = aim.target_ptr;
        } else {
            std::lock_guard<std::mutex> lk(s.mtx);
            s.aim_target = 0;
        }
        ApplyMisc(dma, local);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void HotkeyThread(){
    while(true){
        if(GetAsyncKeyState(VK_F1)&1) Cfg::esp_enabled  = !Cfg::esp_enabled;
        if(GetAsyncKeyState(VK_F2)&1) Cfg::aim_enabled  = !Cfg::aim_enabled;
        if(GetAsyncKeyState(VK_F3)&1) Cfg::inf_stamina  = !Cfg::inf_stamina;
        if(GetAsyncKeyState(VK_F4)&1) Cfg::inf_hunger   = !Cfg::inf_hunger;
        if(GetAsyncKeyState(VK_F5)&1) Cfg::inf_thirst   = !Cfg::inf_thirst;
        if(GetAsyncKeyState(VK_F6)&1) Cfg::esp_ai       = !Cfg::esp_ai;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static void die(const char* msg){
    printf("\n[FATAL] %s\n", msg);
    printf("press enter to exit...\n");
    getchar();
}

int main(){
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

    puts("[*] TICheat starting...");

    auto chk = [](const char* dll) -> bool {
        HMODULE h = LoadLibraryA(dll);
        if(!h) return false;
        FreeLibrary(h); return true;
    };
    if(!chk("vmm.dll"))       { die("vmm.dll not found"); return 1; }
    if(!chk("leechcore.dll")) { die("leechcore.dll not found"); return 1; }
    puts("[+] DLLs found");

    DMA dma;
    if(!dma.Init()){ die("DMA init failed"); return 1; }
    puts("[+] DMA init OK");

    if(!dma.AttachProcess(L"TheIsleClient-Win64-Shipping.exe")){
        die("process not found — launch game first"); return 2;
    }
    puts("[+] attached to TheIsle");

    BootstrapResult br = Bootstrap(dma);
    if(!br.ok){
        printf("[FATAL] bootstrap failed  base=%llX  world=%llX  names=%llX\n",
            br.module_base, br.gworld, br.gnames);
        die("bootstrap failed");
        return 3;
    }
    printf("[+] base=%llX  GWorld=%llX  GNames=%llX\n",
        br.module_base, br.gworld, br.gnames);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    printf("[*] screen %dx%d\n", sw, sh);

    Overlay overlay;
    if(!overlay.Init(sw, sh)){ die("overlay init failed"); return 4; }
    puts("[+] overlay ready");

    SharedState state;
    std::thread t1(EntityThread, std::ref(dma), std::ref(br), std::ref(state));
    std::thread t2(AimThread,    std::ref(dma), std::ref(state));
    std::thread t3(HotkeyThread);
    t1.detach(); t2.detach(); t3.detach();

    puts("[+] threads running -- F1=ESP F2=aim F3=stamina F4=hunger F5=thirst F6=AI");

    MSG msg{};
    while(true){
        while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)){
            if(msg.message == WM_QUIT) goto cleanup;
            DispatchMessage(&msg);
        }
        if(Cfg::esp_enabled){
            std::vector<EntityInfo> ents; CameraState cam;
            uintptr_t local, atgt;
            { std::lock_guard<std::mutex> lk(state.mtx);
              ents  = state.entities;
              cam   = state.cam;
              local = state.local_actor;
              atgt  = state.aim_target; }
            overlay.RenderFrame(ents, cam, local, atgt);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

cleanup:
    return 0;
}
