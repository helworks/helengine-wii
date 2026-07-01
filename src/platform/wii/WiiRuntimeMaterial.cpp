#include "platform/wii/WiiRuntimeMaterial.hpp"

#include <utility>

namespace helengine::wii {
    /// Creates one Wii runtime material with authored-color defaults that remain visible when cooking is incomplete.
    WiiRuntimeMaterial::WiiRuntimeMaterial()
        : BaseColorValue(1.0f, 1.0f, 1.0f)
        , TextureRelativePathValue()
        , OwnedDiffuseTextureValue(nullptr) {
    }

    /// Gets the normalized authored base RGB color resolved from the cooked platform material.
    float3 WiiRuntimeMaterial::GetBaseColor() const {
        return BaseColorValue;
    }

    /// Replaces the normalized authored base RGB color used by the GX submission paths.
    void WiiRuntimeMaterial::SetBaseColor(float3 value) {
        BaseColorValue = value;
    }

    /// Gets the cooked diffuse-texture path resolved from the packaged platform material payload.
    const std::string& WiiRuntimeMaterial::GetTextureRelativePath() const {
        return TextureRelativePathValue;
    }

    /// Replaces the cooked diffuse-texture path used to validate authored textured submissions.
    void WiiRuntimeMaterial::SetTextureRelativePath(std::string value) {
        TextureRelativePathValue = std::move(value);
    }

    /// Gets the runtime diffuse texture owned directly by this material when the path-based cooked-material contract loads one internally.
    RuntimeTexture* WiiRuntimeMaterial::GetOwnedDiffuseTexture() const {
        return OwnedDiffuseTextureValue;
    }

    /// Replaces the runtime diffuse texture owned directly by this material when the path-based cooked-material contract loads one internally.
    void WiiRuntimeMaterial::SetOwnedDiffuseTexture(RuntimeTexture* value) {
        OwnedDiffuseTextureValue = value;
    }
}
