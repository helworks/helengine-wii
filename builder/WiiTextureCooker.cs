namespace helengine.wii.builder;

/// <summary>
/// Cooks shared-engine texture assets into Wii-native serialized texture payloads.
/// </summary>
public sealed class WiiTextureCooker {
    /// <summary>
    /// Cooks one shared-engine texture asset into a Wii-native texture asset.
    /// </summary>
    /// <param name="sourceTexture">Shared-engine source texture asset.</param>
    /// <param name="settings">Resolved Wii texture cook settings.</param>
    /// <returns>Wii-native cooked texture asset.</returns>
    public TextureAsset CookTexture(TextureAsset sourceTexture, WiiTextureCookSettings settings) {
        if (sourceTexture == null) {
            throw new ArgumentNullException(nameof(sourceTexture));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        } else if (!string.Equals(settings.ColorFormatId, TextureAssetColorFormat.GxRgb5A3.ToString(), StringComparison.Ordinal)) {
            throw new InvalidOperationException($"Wii texture cooking currently supports only {TextureAssetColorFormat.GxRgb5A3}.");
        }

        TextureAsset workingTexture = CloneTextureAsset(sourceTexture);
        if (workingTexture.Width == 0 || workingTexture.Height == 0) {
            throw new InvalidOperationException("Wii textures require nonzero dimensions.");
        }

        if (workingTexture.ColorFormat == TextureAssetColorFormat.GxRgb5A3) {
            if (settings.MaxResolution > 0 && (workingTexture.Width > settings.MaxResolution || workingTexture.Height > settings.MaxResolution)) {
                throw new InvalidOperationException("Wii textures cannot resize prepacked GxRgb5A3 payloads. The editor must provide an RGBA32 source texture when downscaling is required.");
            }

            workingTexture.AlphaPrecision = settings.AlphaPrecision;
            return workingTexture;
        } else if (workingTexture.ColorFormat != TextureAssetColorFormat.Rgba32) {
            throw new InvalidOperationException($"Wii texture cooking requires RGBA32 source textures, but got '{workingTexture.ColorFormat}'.");
        }

        if (settings.MaxResolution > 0) {
            workingTexture = ResizeTextureIfNeeded(workingTexture, settings.MaxResolution);
        }

        workingTexture.AlphaPrecision = settings.AlphaPrecision;
        byte[] packedColors = EncodeRgba32ToGxRgb5A3(workingTexture);
        return new TextureAsset {
            Id = workingTexture.Id,
            RuntimeAssetId = workingTexture.RuntimeAssetId,
            Width = workingTexture.Width,
            Height = workingTexture.Height,
            Colors = packedColors,
            PaletteColors = Array.Empty<byte>(),
            ColorFormat = TextureAssetColorFormat.GxRgb5A3,
            AlphaPrecision = settings.AlphaPrecision,
            IsEngineOwned = workingTexture.IsEngineOwned
        };
    }

    /// <summary>
    /// Clones one source texture asset so builder-owned cooking never mutates the shared input payload.
    /// </summary>
    /// <param name="sourceTexture">Shared-engine source texture asset.</param>
    /// <returns>Independent clone of the supplied texture asset.</returns>
    static TextureAsset CloneTextureAsset(TextureAsset sourceTexture) {
        if (sourceTexture == null) {
            throw new ArgumentNullException(nameof(sourceTexture));
        }

        return new TextureAsset {
            Id = sourceTexture.Id,
            RuntimeAssetId = sourceTexture.RuntimeAssetId,
            Width = sourceTexture.Width,
            Height = sourceTexture.Height,
            Colors = sourceTexture.Colors == null ? Array.Empty<byte>() : [.. sourceTexture.Colors],
            PaletteColors = sourceTexture.PaletteColors == null ? Array.Empty<byte>() : [.. sourceTexture.PaletteColors],
            ColorFormat = sourceTexture.ColorFormat,
            AlphaPrecision = sourceTexture.AlphaPrecision,
            IsEngineOwned = sourceTexture.IsEngineOwned
        };
    }

    /// <summary>
    /// Downscales one RGBA32 texture asset with nearest-neighbor sampling when it exceeds the configured maximum resolution.
    /// </summary>
    /// <param name="sourceTexture">Source RGBA32 texture asset.</param>
    /// <param name="maxResolution">Maximum allowed width or height in pixels.</param>
    /// <returns>The original texture when no resize is needed; otherwise one downscaled RGBA32 texture asset.</returns>
    static TextureAsset ResizeTextureIfNeeded(TextureAsset sourceTexture, int maxResolution) {
        if (sourceTexture == null) {
            throw new ArgumentNullException(nameof(sourceTexture));
        } else if (maxResolution < 1) {
            throw new ArgumentOutOfRangeException(nameof(maxResolution), "Maximum resolution must be positive.");
        }

        int sourceWidth = sourceTexture.Width;
        int sourceHeight = sourceTexture.Height;
        int dominantDimension = Math.Max(sourceWidth, sourceHeight);
        if (dominantDimension <= maxResolution) {
            return sourceTexture;
        }

        double scale = (double)maxResolution / dominantDimension;
        int targetWidth = Math.Max(1, (int)Math.Round(sourceWidth * scale));
        int targetHeight = Math.Max(1, (int)Math.Round(sourceHeight * scale));
        byte[] resizedColors = new byte[targetWidth * targetHeight * 4];

        for (int targetY = 0; targetY < targetHeight; targetY++) {
            int sourceY = Math.Min(sourceHeight - 1, (int)Math.Floor(targetY / scale));
            for (int targetX = 0; targetX < targetWidth; targetX++) {
                int sourceX = Math.Min(sourceWidth - 1, (int)Math.Floor(targetX / scale));
                int sourceOffset = ((sourceY * sourceWidth) + sourceX) * 4;
                int targetOffset = ((targetY * targetWidth) + targetX) * 4;
                resizedColors[targetOffset + 0] = sourceTexture.Colors[sourceOffset + 0];
                resizedColors[targetOffset + 1] = sourceTexture.Colors[sourceOffset + 1];
                resizedColors[targetOffset + 2] = sourceTexture.Colors[sourceOffset + 2];
                resizedColors[targetOffset + 3] = sourceTexture.Colors[sourceOffset + 3];
            }
        }

        return new TextureAsset {
            Id = sourceTexture.Id,
            RuntimeAssetId = sourceTexture.RuntimeAssetId,
            Width = (ushort)targetWidth,
            Height = (ushort)targetHeight,
            Colors = resizedColors,
            PaletteColors = Array.Empty<byte>(),
            ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = sourceTexture.AlphaPrecision,
            IsEngineOwned = sourceTexture.IsEngineOwned
        };
    }

    /// <summary>
    /// Encodes one logical RGBA32 texture into tiled Wii GX RGB5A3 bytes.
    /// </summary>
    /// <param name="sourceTexture">RGBA32 source texture asset.</param>
    /// <returns>Tiled Wii-endian RGB5A3 bytes.</returns>
    static byte[] EncodeRgba32ToGxRgb5A3(TextureAsset sourceTexture) {
        if (sourceTexture == null) {
            throw new ArgumentNullException(nameof(sourceTexture));
        } else if (sourceTexture.Colors == null) {
            throw new InvalidOperationException("Wii texture cooking requires RGBA32 color data.");
        }

        int width = sourceTexture.Width;
        int height = sourceTexture.Height;
        int expectedColorByteCount = width * height * 4;
        if (sourceTexture.Colors.Length != expectedColorByteCount) {
            throw new InvalidOperationException("Wii texture cooking requires tightly packed RGBA32 source bytes.");
        }

        int paddedWidth = (width + 3) & ~3;
        int paddedHeight = (height + 3) & ~3;
        byte[] destination = new byte[paddedWidth * paddedHeight * 2];
        int destinationOffset = 0;

        for (int blockY = 0; blockY < paddedHeight; blockY += 4) {
            for (int blockX = 0; blockX < paddedWidth; blockX += 4) {
                for (int innerY = 0; innerY < 4; innerY++) {
                    for (int innerX = 0; innerX < 4; innerX++) {
                        int sampleX = Math.Min(blockX + innerX, width - 1);
                        int sampleY = Math.Min(blockY + innerY, height - 1);
                        int sourceOffset = ((sampleY * width) + sampleX) * 4;
                        byte alpha = QuantizeAlpha(sourceTexture.Colors[sourceOffset + 3], sourceTexture.AlphaPrecision);
                        ushort packedPixel = EncodeRgb5A3Pixel(
                            sourceTexture.Colors[sourceOffset + 0],
                            sourceTexture.Colors[sourceOffset + 1],
                            sourceTexture.Colors[sourceOffset + 2],
                            alpha);
                        destination[destinationOffset + 0] = (byte)(packedPixel >> 8);
                        destination[destinationOffset + 1] = (byte)(packedPixel & 0xFF);
                        destinationOffset += 2;
                    }
                }
            }
        }

        return destination;
    }

    /// <summary>
    /// Converts one logical RGBA texel into one packed GX RGB5A3 pixel value.
    /// </summary>
    /// <param name="red">Red channel in 8-bit range.</param>
    /// <param name="green">Green channel in 8-bit range.</param>
    /// <param name="blue">Blue channel in 8-bit range.</param>
    /// <param name="alpha">Alpha channel in 8-bit range.</param>
    /// <returns>Packed GX RGB5A3 pixel value.</returns>
    static ushort EncodeRgb5A3Pixel(byte red, byte green, byte blue, byte alpha) {
        if (alpha >= 224) {
            return (ushort)(
                0x8000
                | (Convert8To5(red) << 10)
                | (Convert8To5(green) << 5)
                | Convert8To5(blue));
        }

        return (ushort)(
            (Convert8To3(alpha) << 12)
            | (Convert8To4(red) << 8)
            | (Convert8To4(green) << 4)
            | Convert8To4(blue));
    }

    /// <summary>
    /// Converts one 8-bit channel into the 5-bit range used by opaque GX RGB5A3 texels.
    /// </summary>
    /// <param name="value">8-bit color channel value.</param>
    /// <returns>Converted 5-bit channel value.</returns>
    static int Convert8To5(byte value) {
        return ((value * 31) + 127) / 255;
    }

    /// <summary>
    /// Converts one 8-bit channel into the 4-bit range used by translucent GX RGB5A3 texels.
    /// </summary>
    /// <param name="value">8-bit color channel value.</param>
    /// <returns>Converted 4-bit channel value.</returns>
    static int Convert8To4(byte value) {
        return ((value * 15) + 127) / 255;
    }

    /// <summary>
    /// Converts one 8-bit alpha channel into the 3-bit range used by translucent GX RGB5A3 texels.
    /// </summary>
    /// <param name="value">8-bit alpha channel value.</param>
    /// <returns>Converted 3-bit alpha value.</returns>
    static int Convert8To3(byte value) {
        return ((value * 7) + 127) / 255;
    }

    /// <summary>
    /// Quantizes one 8-bit alpha value to the requested storage precision.
    /// </summary>
    /// <param name="alpha">Authored 8-bit alpha value.</param>
    /// <param name="alphaPrecision">Alpha precision to apply.</param>
    /// <returns>Quantized 8-bit alpha value.</returns>
    static byte QuantizeAlpha(byte alpha, TextureAssetAlphaPrecision alphaPrecision) {
        if (alphaPrecision == TextureAssetAlphaPrecision.Opaque) {
            return byte.MaxValue;
        } else if (alphaPrecision == TextureAssetAlphaPrecision.Binary) {
            return alpha >= 128 ? byte.MaxValue : (byte)0;
        } else if (alphaPrecision == TextureAssetAlphaPrecision.A4) {
            return (byte)((alpha & 0xF0) | (alpha >> 4));
        }

        return alpha;
    }
}
