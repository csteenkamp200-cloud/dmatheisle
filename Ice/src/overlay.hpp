#pragma once
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include "math.hpp"
#include "esp_entity.hpp"
#include "camera.hpp"
#include "cheat.hpp"
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dwrite")

class Overlay {
public:
    HWND hwnd=nullptr;
    ID2D1Factory* d2d=nullptr;
    ID2D1HwndRenderTarget* rt=nullptr;
    IDWriteFactory* dwrite=nullptr;
    IDWriteTextFormat* font=nullptr;
    int width=0,height=0;
    ID2D1SolidColorBrush *br_white=nullptr,*br_red=nullptr,
        *br_green=nullptr,*br_yellow=nullptr,*br_bg=nullptr;

    bool Init(int w,int h){
        width=w; height=h;
        WNDCLASSEXA wc{sizeof(wc)};
        wc.lpfnWndProc=DefWindowProcA;
        wc.hInstance=GetModuleHandleA(nullptr);
        wc.lpszClassName="TIOverlay";
        RegisterClassExA(&wc);
        hwnd=CreateWindowExA(WS_EX_TOPMOST|WS_EX_TRANSPARENT|WS_EX_LAYERED,
            "TIOverlay",nullptr,WS_POPUP,0,0,w,h,nullptr,nullptr,nullptr,nullptr);
        SetLayeredWindowAttributes(hwnd,RGB(0,0,0),0,LWA_COLORKEY);
        ShowWindow(hwnd,SW_SHOW);
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&d2d);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),(IUnknown**)&dwrite);
        auto rtp=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,D2D1_ALPHA_MODE_PREMULTIPLIED));
        auto hwp=D2D1::HwndRenderTargetProperties(hwnd,D2D1::SizeU(w,h));
        d2d->CreateHwndRenderTarget(rtp,hwp,&rt);
        dwrite->CreateTextFormat(L"Consolas",nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,12.f,L"en-us",&font);
        rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1,1),&br_white);
        rt->CreateSolidColorBrush(D2D1::ColorF(1,0.2f,0.2f,1),&br_red);
        rt->CreateSolidColorBrush(D2D1::ColorF(0.2f,1,0.2f,1),&br_green);
        rt->CreateSolidColorBrush(D2D1::ColorF(1,1,0,1),&br_yellow);
        rt->CreateSolidColorBrush(D2D1::ColorF(0,0,0,0.45f),&br_bg);
        return rt != nullptr;
    }

    void DrawLabel2D(float x,float y,const wchar_t* txt,ID2D1SolidColorBrush* br){
        D2D1_RECT_F r=D2D1::RectF(x,y,x+500,y+20);
        rt->DrawText(txt,(UINT32)wcslen(txt),font,r,br);
    }

    void RenderFrame(const std::vector<EntityInfo>& ents,
        const CameraState& cam, uintptr_t local_ptr, uintptr_t aim_tgt)
    {
        if(!rt) return;
        rt->BeginDraw();
        rt->Clear(D2D1::ColorF(0,0,0,0));
        float fov=cam.fov>0?cam.fov:90.f;
        for(auto& e:ents){
            if(e.actor_ptr==local_ptr) continue;
            if(e.is_dead&&!Cfg::esp_dead) continue;
            if(e.is_ai&&!Cfg::esp_ai) continue;
            if(!e.is_ai&&!Cfg::esp_players) continue;
            double dist=Dist3D(cam.pos,e.world_pos)*0.01; // cm → m
            if(dist>(double)Cfg::esp_max_dist*0.01) continue;
            double head_z = e.growth>0.1f ? 80.0+e.growth*120.0 : 180.0;
            FVector feet = e.world_pos;
            FVector head = { feet.x, feet.y, feet.z + head_z };
            Vec2 sf,sh;
            if(!WorldToScreen(feet,cam.pos,cam.yaw,cam.pitch,fov,width,height,sf)) continue;
            WorldToScreen(head,cam.pos,cam.yaw,cam.pitch,fov,width,height,sh);
            float bh=fabsf(sf.y-sh.y), bw=bh*0.4f;
            if(bh<3.f) continue;
            auto col=e.is_ai?br_yellow:(e.actor_ptr==aim_tgt?br_red:br_white);
            if(Cfg::esp_box)
                rt->DrawRectangle(D2D1::RectF(sh.x-bw*.5f,sh.y,sh.x+bw*.5f,sh.y+bh),col,1.f);
            if(Cfg::esp_healthbar&&e.max_health>0){
                float ratio=e.health/e.max_health;
                float bx=sh.x-bw*.5f-6.f;
                auto hcol=ratio>.5f?br_green:(ratio>.25f?br_yellow:br_red);
                rt->FillRectangle(D2D1::RectF(bx-2,sh.y,bx,sh.y+bh),br_bg);
                rt->FillRectangle(D2D1::RectF(bx-2,sh.y+bh-bh*ratio,bx,sh.y+bh),hcol);
            }
            if(Cfg::esp_name){
                wchar_t label[256]; wchar_t nw[128];
                if(e.player_name[0]) mbstowcs_s(nullptr,nw,e.player_name,127);
                else wcscpy_s(nw,e.is_ai?L"AI":L"?");
                swprintf_s(label,L"%s  %.0fm  G%.2f  HP%.0f/%.0f",
                    nw,(float)dist,e.growth,e.health,e.max_health);
                DrawLabel2D(sh.x-bw*.5f,sh.y-15.f,label,col);
            }
        }
        rt->EndDraw();
    }
};
