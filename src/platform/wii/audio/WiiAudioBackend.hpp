#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "AudioAsset.hpp"
#include "AudioPlaybackRequest.hpp"
#include "IAudioBackend.hpp"

namespace helengine::wii {
    /// <summary>
    /// Plays shared Helengine PCM assets through the libogc ASND mixer.
    /// </summary>
    class WiiAudioBackend final : public ::IAudioBackend {
    public:
        /// <summary>
        /// Initializes the Wii audio mixer and default bus gains.
        /// </summary>
        WiiAudioBackend();

        /// <summary>
        /// Stops active voices and tears down the Wii audio mixer.
        /// </summary>
        ~WiiAudioBackend();

        int32_t Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) override;

        void Stop(int32_t voiceId) override;

        void SetBusGain(std::string busId, float gain) override;

        void SetBusPaused(std::string busId, bool paused) override;

        bool IsPlaying(int32_t voiceId) override;

        void Update() override;

    private:
        struct ActiveVoiceState {
            int32_t VoiceId;
            int32_t Slot;
            std::string BusId;
            float BaseGain;
            bool Paused;
            void* AudioBuffer;
            int32_t AudioBufferByteLength;
        };

        static std::string NormalizeBusId(std::string busId);

        static float ClampGain(float gain);

        static int32_t ConvertGainToVolume(float gain);

        static int32_t ResolveVoiceFormat(const ::AudioAsset* asset);

        int32_t AcquireVoiceSlot() const;

        float ResolveCombinedGain(const std::string& busId, float baseGain) const;

        bool IsBusPaused(const std::string& busId) const;

        void ApplyVoicePlaybackState(ActiveVoiceState& voice);

        void ReleaseVoiceState(ActiveVoiceState& voice, bool stopNativeVoice);

        std::unordered_map<int32_t, ActiveVoiceState> ActiveVoicesById;
        std::unordered_map<std::string, float> BusGainsById;
        std::unordered_set<std::string> PausedBusIds;
    };
}
