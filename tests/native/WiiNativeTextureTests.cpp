#include "platform/wii/WiiNativeTextureLayout.hpp"
#include <cassert>
#include <iostream>
#include <vector>

/// Writes fixed-format fixture words in the documented CPU header order.
static void PutWord(std::vector<std::uint8_t>& bytes,std::size_t offset,std::uint32_t word) {
    for(int i=0;i<4;i++)bytes[offset+i]=word>>(i*8);
}
/// Builds a header independently from production parsing for one native tile fixture.
static std::vector<std::uint8_t> Payload(std::uint32_t format,std::uint32_t pixels,std::uint32_t tlut,std::uint32_t entries,std::uint32_t alpha) {
    std::vector<std::uint8_t> data(48+pixels,0);
    PutWord(data,0,0x314e5847);PutWord(data,4,1);PutWord(data,8,format);PutWord(data,12,3);PutWord(data,16,5);
    PutWord(data,20,pixels);PutWord(data,24,tlut);PutWord(data,28,entries);PutWord(data,32,entries*2);PutWord(data,36,alpha);
    return data;
}
/// Validates literal tile sizes, header mutations, full fourteen-bit palettes and direct region packing.
int main() {
    using namespace helengine::wii;
    const std::uint32_t formats[]={0,1,2,3,4,5,6,8,9,10,14,17,19,22};
    const std::uint32_t lengths[]={32,64,64,64,64,64,128,32,64,64,32,64,64,128};
    const std::uint32_t alphas[]={2,3,2,3,0,5,3,3,3,3,1,3,3,0};
    std::size_t selectors=0;
    for(std::size_t i=0;i<14;i++) {
        const bool indexed=formats[i]==8||formats[i]==9||formats[i]==10;
        for(std::uint32_t tlut=0;tlut<(indexed?3u:1u);tlut++) {
            auto bytes=Payload(formats[i],lengths[i],indexed?tlut:UINT32_MAX,indexed?16:0,indexed?(tlut==1?0:tlut==2?5:3):alphas[i]);
            std::vector<std::uint8_t> palette(indexed?32:0,0);
            WiiNativeTextureLayout layout;
            assert(TryParseNativeTexturePayload(bytes.data(),bytes.size(),palette.data(),palette.size(),layout));
            assert(layout.Width==3&&layout.Height==5&&layout.PixelBytes==lengths[i]&&layout.NativeFormat==formats[i]);
            if(formats[i]==17)assert(layout.SamplerFormat==1);
            if(formats[i]==19)assert(layout.SamplerFormat==3);
            if(formats[i]==22)assert(layout.SamplerFormat==6);
            auto damaged=bytes;PutWord(damaged,40,1);
            assert(!TryParseNativeTexturePayload(damaged.data(),damaged.size(),palette.data(),palette.size(),layout));
            assert(layout.PixelBytes==0);
            assert(!TryParseNativeTexturePayload(bytes.data(),bytes.size()-1,palette.data(),palette.size(),layout));
            damaged=bytes;damaged.push_back(0);assert(!TryParseNativeTexturePayload(damaged.data(),damaged.size(),palette.data(),palette.size(),layout));
            selectors++;
        }
    }
    assert(selectors==20);
    auto full=Payload(10,64,2,16384,5);full[48]=0x3f;full[49]=0xff;
    std::vector<std::uint8_t> palette(32768,0);palette.back()=0xff;
    WiiNativeTextureLayout layout;
    assert(TryParseNativeTexturePayload(full.data(),full.size(),palette.data(),palette.size(),layout));
    WiiNativeTlutBinding binding;
    assert(TryBuildNativeTlutBinding(layout,0,binding));
    assert(binding.Name==16&&binding.TmemAddress==0xe0000&&binding.LoadRegister==0x65100300);
    assert(TryBuildNativeTlutBinding(layout,3,binding)&&binding.TmemAddress==0xf8000&&binding.Name==19);
    assert(!TryBuildNativeTlutBinding(layout,4,binding));
    full[48]=0x40;assert(!TryParseNativeTexturePayload(full.data(),full.size(),palette.data(),palette.size(),layout));
    full[48]=0;full[49]=16;PutWord(full,28,16);PutWord(full,32,32);
    assert(!TryParseNativeTexturePayload(full.data(),full.size(),palette.data(),32,layout));
    full[49]=15;assert(TryParseNativeTexturePayload(full.data(),full.size(),palette.data(),32,layout));
    assert(TryBuildNativeTlutBinding(layout,0,binding)&&binding.LoadRegister==0x65000700); // Actual sixteen entries, not capacity.
    assert(!TryBuildNativeTextureLayout(7,3,5,UINT32_MAX,0,0,layout));
    assert(!TryBuildNativeTextureLayout(6,1025,1,UINT32_MAX,0,3,layout));
    assert(!TryBuildNativeTextureLayout(6,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,3,layout));
    assert(TryBuildNativeTextureLayout(6,1024,1024,UINT32_MAX,0,3,layout)&&layout.PixelBytes==4194304);
    const std::uint8_t red[]={255,0,0,128};
    assert(TryBuildNativeTextureLayout(4,3,5,UINT32_MAX,0,0,layout));
    std::vector<std::uint8_t> pixels(layout.PixelBytes,0x55);
    assert(PatchNativeTextureRegion(layout,pixels.data(),pixels.size(),2,4,1,1,red,sizeof(red),4));
    assert(pixels[36]==0xf8&&pixels[37]==0&&pixels[38]==0xf8&&pixels[39]==0); // Right/bottom padding repeats the edge.
    assert(pixels[0]==0x55);
    auto unchanged=pixels;
    assert(!PatchNativeTextureRegion(layout,pixels.data(),pixels.size(),3,4,1,1,red,sizeof(red),4)&&pixels==unchanged);
    assert(!PatchNativeTextureRegion(layout,pixels.data(),pixels.size(),2,4,1,1,red,3,4)&&pixels==unchanged);
    assert(TryBuildNativeTextureLayout(6,3,5,UINT32_MAX,0,3,layout));pixels.assign(layout.PixelBytes,0);
    assert(PatchNativeTextureRegion(layout,pixels.data(),pixels.size(),0,0,1,1,red,sizeof(red),4));
    assert(pixels[0]==128&&pixels[1]==255&&pixels[32]==0&&pixels[33]==0); // AR plane then GB plane.
    for(auto immutable:{8u,14u,17u}) {
        assert(TryBuildNativeTextureLayout(immutable,3,5,immutable==8?0:UINT32_MAX,immutable==8?16:0,immutable==8?3:immutable==14?1:3,layout));
        pixels.assign(layout.PixelBytes,0x22);unchanged=pixels;
        assert(!PatchNativeTextureRegion(layout,pixels.data(),pixels.size(),0,0,1,1,red,sizeof(red),4)&&pixels==unchanged);
    }
    std::cout<<"PASS: 20 selectors, strict GXN1/indices, CI14 full palette/eleven-bit load count, native direct updates and immutable rejection.\n";
}
