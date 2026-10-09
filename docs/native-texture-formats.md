# Wii GX texture formats

Wii texture and font-atlas settings publish the same 20 native GX selections
as the shared engine catalog:

- `Gx.I4`, `Gx.I8`, `Gx.IA4`, `Gx.IA8`.
- `Gx.RGB565`, `Gx.RGB5A3`, `Gx.RGBA8`, `Gx.CMPR`.
- `Gx.CI4.TLUT.*`, `Gx.CI8.TLUT.*`, `Gx.CI14X2.TLUT.*`, each with
  `IA8`, `RGB565`, and `RGB5A3` palette encoding.
- Raw depth views `Gx.Z8`, `Gx.Z16`, and `Gx.Z24X8`.

The legacy `GxRgb5A3` selection retains serialized tag 4, packed bytes and
existing alpha conversion. Both image and font-atlas defaults remain
`GxRgb5A3/A8` with `maxResolution=0`, meaning no authored resize cap.
Historical headerless RGB5A3 assets can also be recooked into native direct
or indexed selections without changing the legacy asset contract.
Native settings select their actual encoding; unknown settings fail instead
of silently cooking RGB5A3.

## Shared engine contract

Native selections use `TextureAssetColorFormat.GxNative` (17), the existing
GXN1 descriptor and shared `GxNativeTextureCodec`. No Wii-specific color tag
or duplicate managed codec is introduced. The descriptor consists of twelve
little-endian uint32 words followed by tiled GPU texels. Palette bytes live
in `PaletteColors`. Native tiles and 16-bit TLUT entries are big endian,
independent of the surrounding HELE record's metadata byte order.

The complete descriptor and tile table are documented in
[the GX format contract](../../helengine-gc/docs/native-texture-formats.md).
Logical dimensions must fit within 1..1024 after any authored resizing.
Padding repeats edge texels without enlarging the dimensions submitted to GX.
Existing legacy padded dimensions and their Wii UV compensation are retained.
Native GXN1 textures use their logical dimensions, so they need no padding
compensation in normalized texture coordinates.
Native dimensions that are not powers of two clamp on the corresponding
axis when bound; GX repeat wrapping is retained on power-of-two axes.

RGB5A3 native settings expose actual three-bit alpha (`A3`, stable alpha enum
value 5), Opaque or Binary. I4 and I8 couple sampled alpha to intensity.
CMPR uses GX block ordering and interpolation, with opaque or binary coverage.
Raw depth views encode source R, R/G, or R/G/B bits and sample through the
I8, IA8, or RGBA8 storage interpretation. They do not enable Z-texture writes,
shadow comparisons, render targets or EFB copy operations.

## Palettes, fonts and lifetime

Indexed cooking uses the selected quantizer. The current shared quantizer
supports up to 16 colors for CI4 and 256 colors for CI8 and CI14X2.
`GxNativeTextureCodec.EncodeIndexed` additionally accepts ushort indices and
palettes up to 16384 entries. The Wii runtime supports the complete CI14X2
range and validates reserved bits and indices throughout padded tiles.
Palettes are padded to a multiple of sixteen entries.

Both sprite/font and 3D material binds load the selected native TLUT before
the texture object. Large palettes preserve the eleven-bit load count needed
for 16384 entries despite libogc's narrower region initializer. Aligned owned
buffers retain GPU texels and palette entries after source asset disposal.
Texture release is deferred until a GPU fence, including owned font atlases.

RGBA region updates preserve legacy behavior and update uncompressed native
color formats in place. Indexed, CMPR and raw depth views reject RGBA writes
before mutation. Explicitly cooked atlas work items and manifest atlases,
including legacy tag 4 assets, survive embedded-font externalization with
their selected format and bytes preserved.
Cooking and validation finish before the prior staging directory is reset.

## References and validation scope

Primary contracts:
[libogc GX API](https://github.com/devkitPro/libogc/blob/master/gc/ogc/gx.h),
[libogc GX implementation](https://github.com/devkitPro/libogc/blob/master/libogc/gx.c),
and [Dolphin texture decoder](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/TextureDecoder_Generic.cpp).
Local references are under `C:/dev/helworks/reference/gc`.

Managed tests, actual cooker/native-parser compatibility, GPU-boundary tests,
fresh Wii C++ generation and pinned Wii SDK compilation are separate from
visual validation. No Dolphin execution or hardware rendering is included.
Artifacts remain in `C:/dev/helworks/builds/helengine-wii/texture-formats`.
