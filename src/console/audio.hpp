#pragma once

#include <array>
#include <cstdint>

namespace ooey_station::console {

enum class WaveformType : uint8_t {
    Square   = 0,
    Triangle = 1,
    Sawtooth = 2,
    Sine     = 3,
    Noise    = 4
};

struct AudioChannel {
    bool active{false};
    WaveformType waveform{WaveformType::Square};
    float frequency{440.0f};
    float volume{1.0f};
    double phase{0.0};
    int duration_samples{0};
    uint16_t noise_lfsr{0x1234};
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine() = default;

    void play_tone(int channel, float frequency, int duration_ms, int wave_type);
    void play_sfx(int sfx_id);
    void stop_all();

    // Mix samples into output buffer (stereo interleaved 16-bit PCM)
    void mix(int16_t* buffer, int sample_count);

private:
    static constexpr int SAMPLE_RATE = 44100;
    std::array<AudioChannel, 8> channels_;
};

} // namespace ooey_station::console
