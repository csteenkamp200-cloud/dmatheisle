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
static Overlay         g_overlay;

void EntityThread(DMA& dma, BootstrapResult& br, SharedState& s) {
    int log_tick = 0;

    while (true) {
        uintptr_t gworld = dma.Read<uintptr_t>(br.module_base + Globals::GWorld_Offset);
        uintptr_t level  = dma.Read<uintptr_t>(gworld + Globals::UWorld_PersistentLevel);
        uintptr_t aptr   = dma.Read<uintptr_t>(level  + Globals::ULevel_Actors_Data);
        int32_t   count  = dma.Read<int32_t>  (level  + Globals::ULevel_Actors_Count);

        if (!aptr || count <= 0 || count > 8192) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        std::vector<uintptr_t> actors(count);
        dma.ReadBuf(aptr, actors.data(), (size_t)count * 8);

        std::vector<EntityInfo> frame; frame.reserve(128);
        uintptr_t local = 0;

        for (auto actor : actors) {
            if (!actor) continue;
            std::string cls = GetObjectClassName(dma, br.gnames, actor);
            if (!IsCharacterClass(cls)) continue;
            EntityInfo e{};
            if (!ReadEntity(dma, actor, e)) continue;
            if (!e.is_ai) {
                uintptr_t pc = dma.Read<uintptr_t>(actor + Off::TIChar_PlayerController);
                if (pc) local = actor;
            }
            frame.push_back(e);
        }

        // ── camera (SDK-locked offsets, world chain) ──────────
        CameraState cam{};
        ReadCamera(dma, gworld, cam);

        // ── logging ───────────────────────────────────────────
        if (log_tick++ % 60 == 0) {
            printf("\n[tick] actors=%d  entities=%zu  local=0x%llX\n",
                count, frame.size(), (unsigned long long)local);
            printf("[cam]  pos=(%.0f,%.0f,%.0f)  yaw=%.1f  pitch=%.1f  fov=%.1f\n",
                cam.pos.x, cam.pos.y, cam.pos.z,
                cam.yaw, cam.pitch, cam.fov);
            for (size_t i = 0; i < frame.size() && i < 6; i++) {
                auto& e = frame[i];
                printf("[ent%zu] pos=(%.0f,%.0f,%.0f)  hp=%.0f/%.0f  ai=%d  dead=%d\n",
                    i, e.world_pos.x, e.world_pos.y, e.world_pos.z,
                    e.health, e.max_health, (int)e.is_ai, (int)e.is_dead);
                Vec2 sc{};
                float fov = cam.fov > 0 ? cam.fov : 90.f;
                bool vis = WorldToScreen(e.world_pos, cam.pos,
                    cam.yaw, cam.pitch, fov, 1920, 1080, sc);
                printf("       W2S: vis=%d  screen=(%.0f,%.0f)\n",
                    (int)vis, sc.x, sc.y);
            }
        }

        { std::lock_guard<std::mutex> lk(s.mtx);
          s.entities    = std::move(frame);
          s.cam         = cam;
          s.local_actor = local; }

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}

void AimThread(DMA& dma, SharedState& s) {
    while (true) {
        std::vector<EntityInfo> ents; CameraState cam;
        { std::lock_guard<std::mutex> lk(s.mtx);
          ents = s.entities; cam = s.cam; }
        AimResult aim = RunAimbot(ents, cam);
        if (aim.valid) {
            POINT cur; GetCursorPos(&cur);
            SetCursorPos(cur.x + (int)(aim.delta_yaw   * 10.0),
                         cur.y - (int)(aim.delta_pitch  * 10.0));
            std::lock_guard<std::mutex> lk(s.mtx);
            s.aim_target = aim.target_ptr;
        } else {
            std::lock_guard<std::mutex> lk(s.mtx);
            s.aim_target = 0;
        }
        uintptr_t local;
        { std::lock_guard<std::mutex> lk(s.mtx); local = s.local_actor; }
        ApplyMisc(dma, local);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void HotkeyThread() {
    while (true) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            g_overlay.menu_open = !g_overlay.menu_open;
            g_overlay.SetMenuInput(g_overlay.menu_open);
            printf("[menu] %s\n", g_overlay.menu_open ? "OPEN" : "CLOSED");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static void die(const char* msg) {
    printf("\n[FATAL] %s\n", msg);
    printf("press enter to exit...\n");
    getchar();
}

int main() {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

    puts("[*] TICheat starting...");

    auto chk = [](const char* dll) -> bool {
        HMODULE h = LoadLibraryA(dll);
        if (!h) return false;
        FreeLibrary(h); return true;
    };
    if (!chk("vmm.dll"))       { die("vmm.dll not found");       return 1; }
    if (!chk("leechcore.dll")) { die("leechcore.dll not found"); return 1; }
    puts("[+] DLLs found");

    DMA dma;
    if (!dma.Init())             { die("DMA init failed");  return 1; }
    if (!dma.AttachProcess(L"TheIsleClient-Win64-Shipping.exe"))
                                 { die("process not found"); return 2; }
    puts("[+] attached to TheIsle");

    g_br = Bootstrap(dma);
    if (!g_br.ok) {
        printf("[FATAL] bootstrap failed  base=%llX  world=%llX  names=%llX\n",
            g_br.module_base, g_br.gworld, g_br.gnames);
        die("bootstrap failed"); return 3;
    }
    printf("[+] base=0x%llX  GWorld=0x%llX  GNames=0x%llX\n",
        g_br.module_base, g_br.gworld, g_br.gnames);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    printf("[*] screen %dx%d\n", sw, sh);

    if (!g_overlay.Init(sw, sh)) { die("overlay init failed"); return 4; }
    puts("[+] overlay ready");

    SharedState state;
    std::thread t1(EntityThread, std::ref(dma), std::ref(g_br), std::ref(state));
    std::thread t2(AimThread,    std::ref(dma), std::ref(state));
    std::thread t3(HotkeyThread);
    t1.detach(); t2.detach(); t3.detach();

    puts("[+] running  --  INSERT = menu");

    MSG msg{};
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto cleanup;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        {
            std::vector<EntityInfo> ents; CameraState cam;
            uintptr_t local, atgt;
            { std::lock_guard<std::mutex> lk(state.mtx);
              ents  = state.entities;
              cam   = state.cam;
              local = state.local_actor;
              atgt  = state.aim_target; }
            g_overlay.RenderFrame(ents, cam, local, atgt);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
cleanup:
    return 0;
}
