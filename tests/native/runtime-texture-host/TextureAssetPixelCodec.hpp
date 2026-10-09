#pragma once
#include "TextureAsset.hpp"
/// Boundary spy for generic CPU conversion; native GXN1 tests assert this method is never called.
struct TextureAssetPixelCodec { static Array<std::uint8_t>*DecodeToRgba32(TextureAsset*); };
