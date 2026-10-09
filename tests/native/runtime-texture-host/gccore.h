#pragma once
#include <cstdint>
using u32=std::uint32_t;using u8=std::uint8_t;
/// Host spy representation of the texture descriptor, retaining its actual image pointer and logical sampling dimensions.
struct GXTexObj { void* Data;std::uint16_t Width,Height;std::uint8_t Format;std::uint32_t Tlut;bool Indexed;std::uint8_t WrapS,WrapT; };
/// Host spy representation of one native TLUT descriptor.
struct GXTlutObj { void* Data;std::uint8_t Format;std::uint16_t Entries; };
/// Public libogc region shape; production code corrects its precompiled eleven-bit load count.
struct GXTlutRegion { u32 val[4]{}; };
using GXTlutRegionCallback=GXTlutRegion*(*)(u32);
constexpr u8 GX_TF_RGB5A3=5,GX_REPEAT=1,GX_CLAMP=0,GX_FALSE=0,GX_LINEAR=1,GX_TEXMAP0=0;
constexpr u32 GX_BIGTLUT0=16,GX_TLUT0=0;
constexpr u8 GX_TLUT_16=1;
void GX_InitTexObj(GXTexObj*,void*,std::uint16_t,std::uint16_t,u8,u8,u8,u8);
void GX_InitTexObjCI(GXTexObj*,void*,std::uint16_t,std::uint16_t,u8,u8,u8,u8,u32);
void GX_InitTexObjFilterMode(GXTexObj*,u8,u8);
void GX_InitTexObjWrapMode(GXTexObj*,u8,u8);
u8 GX_GetTexObjWrapS(const GXTexObj*);
u8 GX_GetTexObjWrapT(const GXTexObj*);
void GX_InitTexObjTlut(GXTexObj*,u32);
void GX_InitTlutObj(GXTlutObj*,void*,u8,std::uint16_t);
void GX_InitTlutRegion(GXTlutRegion*,u32,u8);
GXTlutRegionCallback GX_SetTlutRegionCallback(GXTlutRegionCallback);
void GX_LoadTlut(GXTlutObj*,u32);
void GX_LoadTexObj(GXTexObj*,u8);
void GX_InvalidateTexAll();
void GX_DrawDone();
