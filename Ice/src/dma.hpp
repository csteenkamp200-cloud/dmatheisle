#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <cstdio>

using VMM_HANDLE = void*;
#define F_DEFAULT  0ULL
#define F_NOCACHE  0x0008ULL
#define F_NOPAGING 0x0004ULL
#define F_NC_NP    (0x0008ULL|0x0004ULL)

using fn_Init    = VMM_HANDLE(*)(DWORD, LPCSTR*);
using fn_PidGet  = BOOL(*)(VMM_HANDLE, LPSTR, PDWORD);
using fn_ReadEx  = BOOL(*)(VMM_HANDLE, DWORD, ULONG64, PBYTE, DWORD, PDWORD, ULONG64);
using fn_Write   = BOOL(*)(VMM_HANDLE, DWORD, ULONG64, PBYTE, DWORD);
using fn_ModBase = ULONG64(*)(VMM_HANDLE, DWORD, LPWSTR);

static fn_Init    pfn_Init    = nullptr;
static fn_PidGet  pfn_PidGet  = nullptr;
static fn_ReadEx  pfn_ReadEx  = nullptr;
static fn_Write   pfn_Write   = nullptr;
static fn_ModBase pfn_ModBase = nullptr;

static ULONG64 g_read_flag = F_DEFAULT; // 0 confirmed working from probe

class DMA {
public:
    VMM_HANDLE handle = nullptr;
    DWORD      pid    = 0;

    bool Init() {
        HMODULE h = LoadLibraryA("vmm.dll");
        if (!h) { puts("[DMA] vmm.dll not found"); return false; }
        pfn_Init    = (fn_Init)   GetProcAddress(h, "VMMDLL_Initialize");
        pfn_PidGet  = (fn_PidGet) GetProcAddress(h, "VMMDLL_PidGetFromName");
        pfn_ReadEx  = (fn_ReadEx) GetProcAddress(h, "VMMDLL_MemReadEx");
        pfn_Write   = (fn_Write)  GetProcAddress(h, "VMMDLL_MemWrite");
        pfn_ModBase = (fn_ModBase)GetProcAddress(h, "VMMDLL_ProcessGetModuleBaseW");
        if (!pfn_Init||!pfn_PidGet||!pfn_ReadEx||!pfn_Write||!pfn_ModBase) {
            puts("[DMA] missing exports"); return false;
        }
        LPCSTR args[] = { "", "-device", "fpga" };
        handle = pfn_Init(3, args);
        if (!handle) { puts("[DMA] VMMDLL_Initialize failed"); return false; }
        return true;
    }

    bool AttachProcess(const std::wstring& proc) {
        std::string n(proc.begin(), proc.end());
        if (!pfn_PidGet(handle, const_cast<LPSTR>(n.c_str()), &pid) || !pid)
            return false;
        printf("[DMA] attached PID = %u\n", pid);
        return true;
    }

    void ProbeFlags(uintptr_t module_base) {
        struct { ULONG64 flag; const char* name; } combos[] = {
            { F_DEFAULT,  "0 (default)"        },
            { F_NOCACHE,  "NOCACHE"             },
            { F_NOPAGING, "NOPAGING"            },
            { F_NC_NP,    "NOCACHE|NOPAGING"    },
        };
        printf("[DMA] probing read flags at %llX...\n", module_base);
        for (auto& c : combos) {
            uint16_t mz = 0; DWORD rd = 0;
            pfn_ReadEx(handle, pid, (ULONG64)module_base,
                reinterpret_cast<PBYTE>(&mz), 2, &rd, c.flag);
            printf("[DMA]   %-22s rd=%u val=%04X%s\n",
                c.name, rd, mz, (mz==0x5A4D)?" <-- OK":"");
            if (mz == 0x5A4D) { g_read_flag = c.flag; return; }
        }
        puts("[DMA] WARNING: MZ unreadable — card may not be mapped");
    }

    template<typename T>
    T Read(uintptr_t addr) {
        T val{};
        pfn_ReadEx(handle, pid, (ULONG64)addr,
            reinterpret_cast<PBYTE>(&val), sizeof(T), nullptr, g_read_flag);
        return val;
    }

    bool ReadBuf(uintptr_t addr, void* buf, size_t sz) {
        DWORD rd = 0;
        return pfn_ReadEx(handle, pid, (ULONG64)addr,
            reinterpret_cast<PBYTE>(buf), (DWORD)sz, &rd, g_read_flag)
            && rd == (DWORD)sz;
    }

    template<typename T>
    bool Write(uintptr_t addr, const T& val) {
        return pfn_Write(handle, pid, (ULONG64)addr,
            reinterpret_cast<PBYTE>(const_cast<T*>(&val)), sizeof(T));
    }

    uintptr_t GetModuleBase(const std::wstring& mod) {
        return (uintptr_t)pfn_ModBase(handle, pid,
            const_cast<LPWSTR>(mod.c_str()));
    }
};
