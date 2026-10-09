#pragma once
#include "TextureAssetColorFormat.hpp"
#include "runtime/array.hpp"
/// Borrowed source fields copied by the actual production GameCube runtime.
struct TextureAsset { std::uint32_t Width=0,Height=0;TextureAssetColorFormat ColorFormat=TextureAssetColorFormat::Rgba32;std::uint32_t AlphaPrecision=0;Array<std::uint8_t>*Colors=nullptr;Array<std::uint8_t>*PaletteColors=nullptr; };
