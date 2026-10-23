#include <type_traits>
#include "platform/wii/WiiRenderManager2D.hpp"
#include "platform/wii/audio/WiiAudioBackend.hpp"

/// Verifies native overrides against the actual generated Core ABI without constructing SDK-owned audio or GX objects.
int main() {
    static_assert(!std::is_abstract_v<helengine::wii::WiiAudioBackend>);
    using TextureFactory = RuntimeTexture* (helengine::wii::WiiRenderManager2D::*)(HeCppString, IContentStreamSource*);
    static_assert(std::is_same_v<decltype(&helengine::wii::WiiRenderManager2D::BuildTextureFromCooked), TextureFactory>);
    return 0;
}