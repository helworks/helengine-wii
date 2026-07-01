#pragma once

#include <string>

#include "RuntimeMaterial.hpp"
#include "float3.hpp"

class RuntimeTexture;

namespace helengine::wii {
    /// Carries the minimal Wii-owned material state required by the fixed-function GX raster path.
    class WiiRuntimeMaterial final : public RuntimeMaterial {
    public:
        /// Creates one Wii runtime material with authored-color defaults that remain visible when cooking is incomplete.
        WiiRuntimeMaterial();

        /// Gets the normalized authored base RGB color resolved from the cooked platform material.
        float3 GetBaseColor() const;

        /// Replaces the normalized authored base RGB color used by the GX submission paths.
        void SetBaseColor(float3 value);

        /// Gets the cooked diffuse-texture path resolved from the packaged platform material payload.
        const std::string& GetTextureRelativePath() const;

        /// Replaces the cooked diffuse-texture path used to validate authored textured submissions.
        void SetTextureRelativePath(std::string value);

        /// Gets the runtime diffuse texture owned directly by this material when the path-based cooked-material contract loads one internally.
        RuntimeTexture* GetOwnedDiffuseTexture() const;

        /// Replaces the runtime diffuse texture owned directly by this material when the path-based cooked-material contract loads one internally.
        void SetOwnedDiffuseTexture(RuntimeTexture* value);

    private:
        /// Normalized authored base RGB color copied from the cooked platform material payload.
        float3 BaseColorValue;

        /// Cooked diffuse-texture path copied from the packaged platform material payload.
        std::string TextureRelativePathValue;

        /// Runtime diffuse texture owned by this material when it was loaded through the path-based cooked-material contract.
        RuntimeTexture* OwnedDiffuseTextureValue;
    };
}
