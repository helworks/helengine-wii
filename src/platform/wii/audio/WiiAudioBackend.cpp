#include "platform/wii/audio/WiiAudioBackend.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <malloc.h>
#include <stdexcept>
#include <utility>

#include <asndlib.h>

namespace helengine::wii {
    namespace {
        constexpr int32_t RequiredAudioAlignment = 32;
    }

    WiiAudioBackend::WiiAudioBackend()
        : ActiveVoicesById(),
          BusGainsById(),
          PausedBusIds() {
        BusGainsById.emplace("master", 1.0f);
        BusGainsById.emplace("music", 1.0f);
        BusGainsById.emplace("sfx", 1.0f);

        ASND_Init();
        ASND_Pause(0);
    }

    WiiAudioBackend::~WiiAudioBackend() {
        for (auto& voiceEntry : ActiveVoicesById) {
            ReleaseVoiceState(voiceEntry.second, true);
        }

        ActiveVoicesById.clear();
        ASND_End();
    }

    int32_t WiiAudioBackend::Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) {
        if (asset == nullptr) {
            throw std::invalid_argument("asset");
        }
        if (asset->SampleRate <= 0) {
            throw std::runtime_error("Wii audio playback requires a positive sample rate.");
        }
        if (asset->EncodingFamilyId != "pcm-streamed") {
            throw std::runtime_error("Wii audio playback currently requires shared pcm-streamed assets.");
        }
        if (asset->EncodedBytes == nullptr || asset->EncodedBytes->Length <= 0 || asset->EncodedBytes->Data == nullptr) {
            throw std::runtime_error("Wii audio playback requires one non-empty encoded payload.");
        }
        if ((asset->EncodedBytes->Length % static_cast<int32_t>(sizeof(std::int16_t) * asset->Channels)) != 0) {
            throw std::runtime_error("Wii audio playback requires 16-bit PCM sample alignment.");
        }

        const int32_t voiceFormat = ResolveVoiceFormat(asset);
        const int32_t slot = AcquireVoiceSlot();
        if (slot < 0) {
            throw std::runtime_error("Wii audio playback could not reserve one ASND voice.");
        }

        const int32_t paddedByteLength =
            ((asset->EncodedBytes->Length + (RequiredAudioAlignment - 1)) / RequiredAudioAlignment) * RequiredAudioAlignment;
        void* audioBuffer = memalign(RequiredAudioAlignment, static_cast<std::size_t>(paddedByteLength));
        if (audioBuffer == nullptr) {
            throw std::runtime_error("Wii audio playback could not allocate one aligned ASND buffer.");
        }

        std::memset(audioBuffer, 0, static_cast<std::size_t>(paddedByteLength));
        std::memcpy(audioBuffer, asset->EncodedBytes->Data, static_cast<std::size_t>(asset->EncodedBytes->Length));

        const std::string busId = NormalizeBusId(
            request != nullptr && !request->BusId.empty()
                ? request->BusId
                : asset->DefaultBusId);
        const float baseGain = ClampGain(request != nullptr ? request->Gain : 1.0f);
        const bool loop = request != nullptr ? request->Loop : asset->DefaultLoop;
        const int32_t volume = ConvertGainToVolume(ResolveCombinedGain(busId, baseGain));

        const s32 playResult = loop
            ? ASND_SetInfiniteVoice(slot, voiceFormat, asset->SampleRate, 0, audioBuffer, paddedByteLength, volume, volume)
            : ASND_SetVoice(slot, voiceFormat, asset->SampleRate, 0, audioBuffer, paddedByteLength, volume, volume, nullptr);
        if (playResult != SND_OK) {
            std::free(audioBuffer);
            throw std::runtime_error("Wii audio playback failed to queue the ASND voice.");
        }

        ActiveVoiceState voice = {};
        voice.VoiceId = slot;
        voice.Slot = slot;
        voice.BusId = busId;
        voice.BaseGain = baseGain;
        voice.Paused = false;
        voice.AudioBuffer = audioBuffer;
        voice.AudioBufferByteLength = paddedByteLength;
        ActiveVoicesById[voice.VoiceId] = voice;
        ApplyVoicePlaybackState(ActiveVoicesById[voice.VoiceId]);
        return voice.VoiceId;
    }

    void WiiAudioBackend::Stop(int32_t voiceId) {
        auto voiceIterator = ActiveVoicesById.find(voiceId);
        if (voiceIterator == ActiveVoicesById.end()) {
            return;
        }

        ReleaseVoiceState(voiceIterator->second, true);
        ActiveVoicesById.erase(voiceIterator);
    }

    void WiiAudioBackend::SetBusGain(std::string busId, float gain) {
        BusGainsById[NormalizeBusId(std::move(busId))] = ClampGain(gain);
        for (auto& voiceEntry : ActiveVoicesById) {
            ApplyVoicePlaybackState(voiceEntry.second);
        }
    }

    void WiiAudioBackend::SetBusPaused(std::string busId, bool paused) {
        std::string normalizedBusId = NormalizeBusId(std::move(busId));
        if (paused) {
            PausedBusIds.insert(normalizedBusId);
        } else {
            PausedBusIds.erase(normalizedBusId);
        }

        for (auto& voiceEntry : ActiveVoicesById) {
            ApplyVoicePlaybackState(voiceEntry.second);
        }
    }

    bool WiiAudioBackend::IsPlaying(int32_t voiceId) {
        auto voiceIterator = ActiveVoicesById.find(voiceId);
        if (voiceIterator == ActiveVoicesById.end()) {
            return false;
        }

        s32 status = ASND_StatusVoice(voiceIterator->second.Slot);
        return status != SND_INVALID && status != SND_UNUSED;
    }

    void WiiAudioBackend::Update() {
        for (auto voiceIterator = ActiveVoicesById.begin(); voiceIterator != ActiveVoicesById.end();) {
            s32 status = ASND_StatusVoice(voiceIterator->second.Slot);
            if (status == SND_INVALID || status == SND_UNUSED) {
                ReleaseVoiceState(voiceIterator->second, false);
                voiceIterator = ActiveVoicesById.erase(voiceIterator);
                continue;
            }

            ++voiceIterator;
        }
    }

    std::string WiiAudioBackend::NormalizeBusId(std::string busId) {
        if (busId.empty()) {
            return "master";
        }

        std::transform(
            busId.begin(),
            busId.end(),
            busId.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return busId;
    }

    float WiiAudioBackend::ClampGain(float gain) {
        if (!(gain >= 0.0f) || gain != gain) {
            return 0.0f;
        }

        return std::clamp(gain, 0.0f, 1.0f);
    }

    int32_t WiiAudioBackend::ConvertGainToVolume(float gain) {
        return static_cast<int32_t>(ClampGain(gain) * static_cast<float>(MAX_VOLUME));
    }

    int32_t WiiAudioBackend::ResolveVoiceFormat(const ::AudioAsset* asset) {
        if (asset == nullptr) {
            throw std::invalid_argument("asset");
        }

        switch (asset->Channels) {
            case 1:
                return VOICE_MONO_16BIT_LE;
            case 2:
                return VOICE_STEREO_16BIT_LE;
            default:
                throw std::runtime_error("Wii audio playback currently supports only mono or stereo 16-bit PCM assets.");
        }
    }

    int32_t WiiAudioBackend::AcquireVoiceSlot() const {
        return ASND_GetFirstUnusedVoice();
    }

    float WiiAudioBackend::ResolveCombinedGain(const std::string& busId, float baseGain) const {
        float masterGain = 1.0f;
        auto masterGainIterator = BusGainsById.find("master");
        if (masterGainIterator != BusGainsById.end()) {
            masterGain = masterGainIterator->second;
        }

        float busGain = 1.0f;
        auto busGainIterator = BusGainsById.find(busId);
        if (busGainIterator != BusGainsById.end()) {
            busGain = busGainIterator->second;
        }

        return ClampGain(masterGain * busGain * baseGain);
    }

    bool WiiAudioBackend::IsBusPaused(const std::string& busId) const {
        return PausedBusIds.contains("master") || PausedBusIds.contains(busId);
    }

    void WiiAudioBackend::ApplyVoicePlaybackState(ActiveVoiceState& voice) {
        bool shouldPause = IsBusPaused(voice.BusId);
        if (shouldPause != voice.Paused) {
            ASND_PauseVoice(voice.Slot, shouldPause ? 1 : 0);
            voice.Paused = shouldPause;
        }

        int32_t volume = ConvertGainToVolume(ResolveCombinedGain(voice.BusId, voice.BaseGain));
        ASND_ChangeVolumeVoice(voice.Slot, volume, volume);
    }

    void WiiAudioBackend::ReleaseVoiceState(ActiveVoiceState& voice, bool stopNativeVoice) {
        if (stopNativeVoice) {
            ASND_StopVoice(voice.Slot);
        }

        if (voice.AudioBuffer != nullptr) {
            std::free(voice.AudioBuffer);
            voice.AudioBuffer = nullptr;
        }

        voice.AudioBufferByteLength = 0;
        voice.Paused = false;
    }
}
