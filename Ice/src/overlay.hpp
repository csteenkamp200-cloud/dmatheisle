#pragma once
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#include <d2d1.h>
#include <dwrite.h>
#include <atomic>
#include <string>
#include <vector>
#include "math.hpp"
#include "esp_entity.hpp"
#include "camera.hpp"
#include "cheat.hpp"
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dwrite")

// atomic<bool>* so we can point at Cfg:: fields directly
struct MenuItem {
    const wchar_t*      label;
    std::atomic<bool>*  value;
};

class Overlay {
public:
    HWND                     hwnd   = nullptr;
    ID2D1Factory*            d2d    = nullptr;
    ID2D1HwndRenderTarget*   rt     = nullptr;
    IDWriteFactory*          dwrite = nullptr;
    IDWriteTextFormat*       font   = nullptr;
    IDWriteTextFormat*       fontHd = nullptr;
    int width=0, height=0;

    ID2D1SolidColorBrush *br_white=nullptr, *br_red=nullptr,
        *br_green=nullptr,  *br_yellow=nullptr,
        *br_bg=nullptr,     *br_panel=nullptr,
        *br_on=nullptr,     *br_off=nullptr,
        *br_header=nullptr;

    bool menu_open = false;
    std::vector<MenuItem> menu_items;

    static Overlay* s_inst;

    static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_LBUTTONDOWN && s_inst && s_inst->menu_open) {
            s_inst->HandleClick(HIWORD(lp));
            return 0;
        }
        if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcA(hw, msg, wp, lp);
    }

    void HandleClick(int mouse_y) {
        int base  = 58;   // first row top-left y (after header)
        int row_h = 36;
        for (int i = 0; i < (int)menu_items.size(); i++) {
            int y0 = base + i * row_h;
            if (mouse_y >= y0 && mouse_y < y0 + row_h) {
                bool cur = menu_items[i].value->load();
                menu_items[i].value->store(!cur);
                break;
            }
        }
    }

    void SetMenuInput(bool enable) {
        LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
        if (enable) ex &= ~WS_EX_TRANSPARENT;
        else        ex |=  WS_EX_TRANSPARENT;
        SetWindowLongA(hwnd, GWL_EXSTYLE, ex);
    }

    bool Init(int w, int h) {
        width=w; height=h;
        s_inst = this;

        // build menu — order = display order
        menu_items = {
            { L"ESP  Master",     &Cfg::esp_enabled   },
            { L"ESP  Players",    &Cfg::esp_players   },
            { L"ESP  AI",         &Cfg::esp_ai        },
            { L"ESP  Dead",       &Cfg::esp_dead      },
            { L"ESP  Box",        &Cfg::esp_box       },
            { L"ESP  Health Bar", &Cfg::esp_healthbar },
            { L"ESP  Names",      &Cfg::esp_name      },
            { L"Aimbot",          &Cfg::aim_enabled   },
            { L"Aim  Hold (RMB)", &Cfg::aim_hold      },
            { L"Aim  AI Only",    &Cfg::aim_ai_only   },
            { L"Inf  Stamina",    &Cfg::inf_stamina   },
            { L"Inf  Hunger",     &Cfg::inf_hunger    },
            { L"Inf  Thirst",     &Cfg::inf_thirst    },
        };

        WNDCLASSEXA wc{sizeof(wc)};
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandleA(nullptr);
        wc.lpszClassName = "TIOverlay";
        RegisterClassExA(&wc);

        hwnd = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
            "TIOverlay", nullptr, WS_POPUP,
            0, 0, w, h, nullptr, nullptr, nullptr, nullptr);
        SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
        ShowWindow(hwnd, SW_SHOW);

        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory), (IUnknown**)&dwrite);

        auto rtp = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
        d2d->CreateHwndRenderTarget(rtp,
            D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(w,h)), &rt);

        dwrite->CreateTextFormat(L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 13.f, L"en-us", &font);
        dwrite->CreateTextFormat(L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 15.f, L"en-us", &fontHd);

        rt->CreateSolidColorBrush(D2D1::ColorF(1.f,1.f,1.f,1.f),    &br_white);
        rt->CreateSolidColorBrush(D2D1::ColorF(1.f,.2f,.2f,1.f),    &br_red);
        rt->CreateSolidColorBrush(D2D1::ColorF(.2f,1.f,.2f,1.f),    &br_green);
        rt->CreateSolidColorBrush(D2D1::ColorF(1.f,1.f,.0f,1.f),    &br_yellow);
        rt->CreateSolidColorBrush(D2D1::ColorF(.0f,.0f,.0f,.55f),    &br_bg);
        rt->CreateSolidColorBrush(D2D1::ColorF(.05f,.05f,.1f,.90f),  &br_panel);
        rt->CreateSolidColorBrush(D2D1::ColorF(.1f,.65f,.1f,.85f),   &br_on);
        rt->CreateSolidColorBrush(D2D1::ColorF(.5f,.08f,.08f,.80f),  &br_off);
        rt->CreateSolidColorBrush(D2D1::ColorF(.1f,.35f,.85f,.95f),  &br_header);
        return rt != nullptr;
    }

    void DrawLabel(float x, float y, const wchar_t* txt,
                   ID2D1SolidColorBrush* br, IDWriteTextFormat* fmt = nullptr) {
        if (!fmt) fmt = font;
        rt->DrawText(txt, (UINT32)wcslen(txt), fmt,
            D2D1::RectF(x, y, x+600, y+22), br);
    }

    void DrawMenu() {
        float pw = 248.f;
        float ph = 46.f + (float)menu_items.size() * 36.f + 8.f;
        float px = 10.f, py = 10.f;

        rt->FillRectangle(D2D1::RectF(px, py, px+pw, py+ph), br_panel);
        rt->DrawRectangle(D2D1::RectF(px, py, px+pw, py+ph), br_header, 1.f);
        rt->FillRectangle(D2D1::RectF(px, py, px+pw, py+36.f), br_header);
        DrawLabel(px+8.f, py+9.f, L"TICheat   INSERT to close", br_white, fontHd);

        float ry = py + 46.f;
        for (auto& item : menu_items) {
            bool on = item.value->load();
            rt->FillRectangle(
                D2D1::RectF(px+5.f, ry+2.f, px+pw-5.f, ry+32.f),
                on ? br_on : br_off);
            rt->DrawRectangle(
                D2D1::RectF(px+5.f, ry+2.f, px+pw-5.f, ry+32.f),
                br_white, .4f);
            DrawLabel(px+12.f, ry+8.f, item.label, br_white);
            DrawLabel(px+pw-46.f, ry+8.f, on ? L"ON" : L"OFF", br_white);
            ry += 36.f;
        }
    }

    void RenderFrame(const std::vector<EntityInfo>& ents,
                     const CameraState& cam,
                     uintptr_t local_ptr, uintptr_t aim_tgt)
    {
        if (!rt) return;
        rt->BeginDraw();
        rt->Clear(D2D1::ColorF(0,0,0,0));

        if (menu_open) DrawMenu();

        if (Cfg::esp_enabled.load()) {
            float fov = cam.fov > 0 ? cam.fov : 90.f;
            for (auto& e : ents) {
                if (e.actor_ptr == local_ptr)              continue;
                if (e.is_dead   && !Cfg::esp_dead.load())   continue;
                if (e.is_ai     && !Cfg::esp_ai.load())     continue;
                if (!e.is_ai    && !Cfg::esp_players.load()) continue;
                double dist = Dist3D(cam.pos, e.world_pos) * 0.01;
                if (dist > (double)Cfg::esp_max_dist * 0.01) continue;
                double hz = e.growth > 0.1f ? 80.0 + e.growth*120.0 : 180.0;
                FVector feet = e.world_pos;
                FVector head = { feet.x, feet.y, feet.z + hz };
                Vec2 sf, sh;
                if (!WorldToScreen(feet, cam.pos, cam.yaw, cam.pitch,
                                   fov, width, height, sf)) continue;
                WorldToScreen(head, cam.pos, cam.yaw, cam.pitch,
                              fov, width, height, sh);
                float bh = fabsf(sf.y - sh.y);
                float bw = bh * 0.4f;
                if (bh < 3.f) continue;
                auto col = e.is_ai ? br_yellow
                         : (e.actor_ptr == aim_tgt ? br_red : br_white);
                if (Cfg::esp_box.load())
                    rt->DrawRectangle(
                        D2D1::RectF(sh.x-bw*.5f, sh.y, sh.x+bw*.5f, sh.y+bh),
                        col, 1.f);
                if (Cfg::esp_healthbar.load() && e.max_health > 0.f) {
                    float ratio = e.health / e.max_health;
                    float bx    = sh.x - bw*.5f - 6.f;
                    auto  hcol  = ratio>.5f ? br_green : ratio>.25f ? br_yellow : br_red;
                    rt->FillRectangle(D2D1::RectF(bx-2,sh.y,bx,sh.y+bh), br_bg);
                    rt->FillRectangle(
                        D2D1::RectF(bx-2,sh.y+bh-bh*ratio,bx,sh.y+bh), hcol);
                }
                if (Cfg::esp_name.load()) {
                    wchar_t label[256], nw[128];
                    if (e.player_name[0]) mbstowcs_s(nullptr,nw,e.player_name,127);
                    else wcscpy_s(nw, e.is_ai ? L"AI" : L"?");
                    swprintf_s(label, L"%s  %.0fm  G%.2f  HP%.0f/%.0f",
                        nw,(float)dist,e.growth,e.health,e.max_health);
                    DrawLabel(sh.x-bw*.5f, sh.y-15.f, label, col);
                }
            }
        }

        // top-right status
        wchar_t st[128];
        swprintf_s(st, L"INSERT=menu  ESP=%s  AIM=%s  fov=%.0f",
            Cfg::esp_enabled.load() ? L"ON" : L"OFF",
            Cfg::aim_enabled.load() ? L"ON" : L"OFF",
            cam.fov);
        DrawLabel((float)(width-500), 6.f, st, br_white);

        rt->EndDraw();
    }
};

inline Overlay* Overlay::s_inst = nullptr;
