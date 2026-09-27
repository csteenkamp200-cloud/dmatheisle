#pragma once
#include "dma.hpp"
#include "globals.hpp"
#include <string>
#include <vector>

inline std::string ResolveFName(DMA& dma, uintptr_t gnames, uint32_t name_index) {
    if (!name_index) return {};
    uint32_t chunk  = name_index >> 16;
    uint32_t offset = name_index & 0xFFFF;
    uintptr_t chunk_ptr = dma.Read<uintptr_t>(
        gnames + Globals::FNamePool_Chunks + (uintptr_t)chunk * 8);
    if (!chunk_ptr) return {};
    uintptr_t entry  = chunk_ptr + (uintptr_t)offset * 2;
    uint16_t  header = dma.Read<uint16_t>(entry);
    uint32_t  len    = header >> 6;
    bool      wide   = (header & 0x1) != 0;
    if (len == 0 || len > 1024) return {};
    if (!wide) {
        std::vector<char> buf(len + 1, 0);
        dma.ReadBuf(entry + 2, buf.data(), len);
        return std::string(buf.data(), len);
    } else {
        std::vector<uint16_t> wbuf(len);
        dma.ReadBuf(entry + 4, wbuf.data(), len * 2);
        std::string out; out.reserve(len);
        for (uint32_t i = 0; i < len; i++)
            out += (wbuf[i] < 128) ? (char)wbuf[i] : '?';
        return out;
    }
}

inline std::string ReadFNameAt(DMA& dma, uintptr_t gnames, uintptr_t fname_addr) {
    uint32_t index = dma.Read<uint32_t>(fname_addr);
    return ResolveFName(dma, gnames, index);
}

inline std::string GetObjectClassName(DMA& dma, uintptr_t gnames, uintptr_t obj) {
    uintptr_t cls = dma.Read<uintptr_t>(obj + Globals::UObject_Class);
    if (!cls) return {};
    return ReadFNameAt(dma, gnames, cls + Globals::UObject_Name);
}
