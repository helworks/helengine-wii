#include "platform/wii/WiiRuntimeTexture.hpp"
#include "TextureAsset.hpp"
#include "TextureAssetPixelCodec.hpp"
#include "runtime/native_exceptions.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

/// Capture order verifies that each indexed bind loads its own full palette before the sampler descriptor.
static std::vector<int> Calls;
/// Current callback and original callback object used to verify stable ownership of unrelated slot names.
static GXTlutRegionCallback Callback=nullptr;
static GXTlutRegion PreviousRegion;
static int CallbackInstalls=0,GenericDecodes=0,Flushes=0,Invalidations=0;
/// Completion count verifies that mutation is fenced, while rejected immutable updates queue no GPU commands.
static int MutationFences=0;
/// Last exact TLUT register and palette contents consumed by the SDK boundary.
static std::uint32_t LoadRegister=0;
static std::vector<std::uint8_t> LoadedPalette;
static GXTexObj BoundTexture;
static GXTlutRegion* OriginalCallback(u32){return &PreviousRegion;}
void GX_InitTexObj(GXTexObj*o,void*d,std::uint16_t w,std::uint16_t h,u8 f,u8 s,u8 t,u8){*o={d,w,h,f,0,false,s,t};}
void GX_InitTexObjCI(GXTexObj*o,void*d,std::uint16_t w,std::uint16_t h,u8 f,u8 s,u8 t,u8 m,u32 n){GX_InitTexObj(o,d,w,h,f,s,t,m);o->Indexed=true;o->Tlut=n;}
void GX_InitTexObjFilterMode(GXTexObj*,u8,u8){}
void GX_InitTexObjWrapMode(GXTexObj*o,u8 s,u8 t){o->WrapS=s;o->WrapT=t;}
u8 GX_GetTexObjWrapS(const GXTexObj*o){return o->WrapS;}
u8 GX_GetTexObjWrapT(const GXTexObj*o){return o->WrapT;}
void GX_InitTexObjTlut(GXTexObj*o,u32 n){o->Tlut=n;}
void GX_InitTlutObj(GXTlutObj*o,void*d,u8 f,std::uint16_t e){*o={d,f,e};}
void GX_InitTlutRegion(GXTlutRegion*r,u32 a,u8 s){r->val[0]=0x65000000|((a-0x80000)>>9)|(u32(s)<<10);}
GXTlutRegionCallback GX_SetTlutRegionCallback(GXTlutRegionCallback c){auto p=Callback;Callback=c;CallbackInstalls++;return p;}
void GX_LoadTlut(GXTlutObj*o,u32 n){Calls.push_back(1);LoadRegister=Callback(n)->val[0];assert(((LoadRegister>>10)&2047)==o->Entries/16);auto*d=static_cast<std::uint8_t*>(o->Data);LoadedPalette.assign(d,d+std::size_t(o->Entries)*2);}
void GX_LoadTexObj(GXTexObj*o,u8){Calls.push_back(2);BoundTexture=*o;}
void GX_InvalidateTexAll(){Invalidations++;}
void GX_DrawDone(){MutationFences++;}
int SYS_Report(const char*,...){return 0;}
void DCFlushRange(void*d,std::size_t n){assert(d!=nullptr&&n>0&&(reinterpret_cast<std::uintptr_t>(d)&31)==0);Flushes++;}
Array<std::uint8_t>*TextureAssetPixelCodec::DecodeToRgba32(TextureAsset*){GenericDecodes++;return new Array<std::uint8_t>(4);}
/// Produces independent little-endian headers around literal big-endian GPU bytes.
static void Word(Array<std::uint8_t>&a,int offset,std::uint32_t word){for(int i=0;i<4;i++)a[offset+i]=word>>(i*8);}
/// Runs production load/bind/update/destruction paths against observable SDK calls.
int main(){
    using namespace helengine::wii;
    Callback=OriginalCallback;
    const std::uint32_t formats[]={0,1,2,3,4,5,6,8,9,10,14,17,19,22};
    const std::uint32_t lengths[]={32,64,64,64,64,64,128,32,64,64,32,64,64,128};
    const std::uint32_t alphas[]={2,3,2,3,0,5,3,3,3,3,1,3,3,0};
    int selectors=0;
    for(int i=0;i<14;i++){
        const bool indexed=formats[i]==8||formats[i]==9||formats[i]==10;
        for(std::uint32_t tlut=0;tlut<(indexed?3u:1u);tlut++){
            const auto entries=indexed?(formats[i]==10?16384u:formats[i]==9?256u:16u):0u;
            const auto alpha=indexed?(tlut==1?0u:tlut==2?5u:3u):alphas[i];
            Array<std::uint8_t> colors(48+lengths[i]),palette(entries*2);
            Word(colors,0,0x314e5847);Word(colors,4,1);Word(colors,8,formats[i]);Word(colors,12,3);Word(colors,16,5);
            Word(colors,20,lengths[i]);Word(colors,24,indexed?tlut:UINT32_MAX);Word(colors,28,entries);Word(colors,32,entries*2);Word(colors,36,alpha);
            if(formats[i]==10){colors[48]=0x3f;colors[49]=0xff;palette[palette.Length-1]=0xab;}
            const std::vector<std::uint8_t> expectedPixels(colors.Data+48,colors.Data+colors.Length);
            const std::vector<std::uint8_t> expectedPalette(palette.Data,palette.Data+palette.Length);
            TextureAsset asset;asset.Width=3;asset.Height=5;asset.ColorFormat=TextureAssetColorFormat::GxNative;asset.AlphaPrecision=alpha;asset.Colors=&colors;asset.PaletteColors=&palette;
            WiiRuntimeTexture texture;texture.LoadFromRaw(&asset);
            assert(texture.get_Width()==3&&texture.get_Height()==5&&texture.GetNativeTextureObject()->Width==3&&texture.GetNativeTextureObject()->Height==5);
            assert(texture.GetNativeTextureWidth()==3&&texture.GetNativeTextureHeight()==5);
            assert(GenericDecodes==0&&std::memcmp(texture.GetNativeTextureObject()->Data,colors.Data+48,lengths[i])==0);
            // Cooked arrays remain caller-owned: replacing their bytes after load cannot alter retained GPU/TLUT storage.
            std::memset(colors.Data+48,0xcd,lengths[i]);std::memset(palette.Data,0xcd,palette.Length);
            GX_InitTexObjWrapMode(texture.GetNativeTextureObject(),GX_REPEAT,GX_REPEAT);
            Calls.clear();texture.BindNativeTexture(0);
            assert(Calls==(indexed?std::vector<int>{1,2}:std::vector<int>{2}));
            assert(std::memcmp(BoundTexture.Data,expectedPixels.data(),expectedPixels.size())==0);
            if(indexed){assert(LoadedPalette==expectedPalette);if(formats[i]==10)assert(LoadRegister==0x65100300&&LoadedPalette.back()==0xab);}
            assert(BoundTexture.Format==(formats[i]==17?1:formats[i]==19?3:formats[i]==22?6:formats[i]));
            assert(BoundTexture.WrapS==GX_CLAMP&&BoundTexture.WrapT==GX_CLAMP);
            if(indexed||formats[i]==14||formats[i]>=17){
                std::vector<std::uint8_t> previous(static_cast<std::uint8_t*>(BoundTexture.Data),static_cast<std::uint8_t*>(BoundTexture.Data)+lengths[i]);
                Array<std::uint8_t> rgba(4);rgba[0]=255;rgba[3]=255;auto flushes=Flushes;auto fences=MutationFences;
                bool rejected=false;try{texture.UpdateRegionFromRgba8(0,0,1,1,&rgba,4);}catch(InvalidOperationException*e){delete e;rejected=true;}
                assert(rejected&&Flushes==flushes&&MutationFences==fences&&std::memcmp(BoundTexture.Data,previous.data(),previous.size())==0);
            }
            if(formats[i]==10){bool rejected=false;Calls.clear();try{texture.BindNativeTexture(4);}catch(InvalidOperationException*e){delete e;rejected=true;}assert(rejected&&Calls.empty());}
            if(formats[i]<=6){
                Array<std::uint8_t> rgba(4);rgba[0]=255;rgba[3]=127;
                const auto* pointer=BoundTexture.Data;const auto flushes=Flushes;const auto fences=MutationFences;
                texture.UpdateRegionFromRgba8(0,0,1,1,&rgba,4);
                assert(texture.GetNativeTextureObject()->Data==pointer&&Flushes==flushes+1&&MutationFences==fences+1);
                const auto* result=static_cast<const std::uint8_t*>(pointer);
                const std::uint8_t first[]={0x50,77,0x75,127,0xf8,0x3f,127};
                assert(result[0]==first[formats[i]]);
                if(formats[i]==3)assert(result[1]==77);
                if(formats[i]==4||formats[i]==5)assert(result[1]==0);
                if(formats[i]==6)assert(result[1]==255&&result[32]==0&&result[33]==0);
            }
            selectors++;
        }
    }
    assert(selectors==20&&CallbackInstalls==1&&Callback(8)==&PreviousRegion);
    Array<std::uint8_t> rgba(4);rgba[0]=255;rgba[3]=127;
    TextureAsset raw;raw.Width=raw.Height=1;raw.Colors=&rgba;
    WiiRuntimeTexture legacy;legacy.LoadFromRaw(&raw);
    assert(legacy.get_Width()==1&&legacy.get_Height()==1&&legacy.GetNativeTextureWidth()==4&&legacy.GetNativeTextureHeight()==4);
    GX_InitTexObjWrapMode(legacy.GetNativeTextureObject(),GX_REPEAT,GX_REPEAT);
    legacy.BindNativeTexture(0);assert(BoundTexture.WrapS==GX_REPEAT&&BoundTexture.WrapT==GX_REPEAT);
    // Existing anonymous font alpha is not overridden by Opaque metadata; RGB5A3 retains three-bit coverage.
    auto*packed=static_cast<std::uint16_t*>(legacy.GetNativeTextureObject()->Data);assert((packed[0]&0x8000)==0&&((packed[0]>>12)&7)==3);
    const auto invalidations=Invalidations;
    rgba[3]=255;legacy.UpdateRegionFromRgba8(0,0,1,1,&rgba,4);assert(packed[0]==0xfc00&&Invalidations==invalidations+1);
    std::cout<<"PASS: actual GX runtime direct bytes/all20 selectors, independent source ownership, logical dimensions, TLUT before sampler/full16384 entries, slot isolation, direct region updates, immutable rejection and anonymous font alpha.\n";
}
