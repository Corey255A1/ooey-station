#include "audio.hpp"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace ooey_station::console {

AudioEngine::AudioEngine() {
    stop_all();
}

void AudioEngine::stop_all() {
    for (auto& ch : channels_) {
        ch.active = false;
        ch.phase = 0.0;
        ch.duration_samples = 0;
        ch.noise_lfsr = 0x1234;
    }
}

void AudioEngine::play_tone(int channel, float frequency, int duration_ms, int wave_type) {
    if (channel < 0 || channel >= 8) return;
    
    auto& ch = channels_[channel];
    ch.waveform = static_cast<WaveformType>(std::clamp(wave_type, 0, 4));
    ch.frequency = frequency;
    ch.volume = 0.25f; // Keep it quiet
    ch.phase = 0.0;
    ch.active = true;
    
    if (duration_ms > 0) {
        ch.duration_samples = (duration_ms * SAMPLE_RATE) / 1000;
    } else {
        ch.duration_samples = -1; // Infinite
    }
}

void AudioEngine::play_sfx(int sfx_id) {
    // Basic pre-defined sound effects for game testing
    if (sfx_id == 0) { // Hit
        play_tone(0, 600.0f, 50, 0); // square wave tone
    } else if (sfx_id == 1) { // Score
        play_tone(0, 400.0f, 150, 1); // triangle wave sweep
    }
}

void AudioEngine::mix(int16_t* buffer, int sample_count) {
    // Initialize mix buffer with zeros
    std::fill(buffer, buffer + sample_count * 2, 0);

    for (int i = 0; i < sample_count; ++i) {
        float mixed_sample = 0.0f;
        
        for (auto& ch : channels_) {
            if (!ch.active) continue;
            
            float sample = 0.0f;
            
            switch (ch.waveform) {
                case WaveformType::Square:
                    sample = (ch.phase < 0.5) ? 1.0f : -1.0f;
                    break;
                case WaveformType::Triangle:
                    if (ch.phase < 0.25) {
                        sample = static_cast<float>(ch.phase * 4.0);
                    } else if (ch.phase < 0.75) {
                        sample = static_cast<float>(2.0 - ch.phase * 4.0);
                    } else {
                        sample = static_cast<float>(ch.phase * 4.0 - 4.0);
                    }
                    break;
                case WaveformType::Sawtooth:
                    sample = static_cast<float>(2.0 * ch.phase - 1.0);
                    break;
                case WaveformType::Sine:
                    sample = static_cast<float>(std::sin(ch.phase * 2.0 * std::numbers::pi));
                    break;
                case WaveformType::Noise: {
                    bool bit = ch.noise_lfsr & 1;
                    ch.noise_lfsr >>= 1;
                    if (bit) ch.noise_lfsr ^= 0xB400;
                    sample = static_cast<float>(ch.noise_lfsr) / 32767.5f - 1.0f;
                    break;
                }
            }
            
            mixed_sample += sample * ch.volume;
            
            // Advance phase
            ch.phase += static_cast<double>(ch.frequency) / SAMPLE_RATE;
            if (ch.phase >= 1.0) {
                ch.phase -= 1.0;
            }
            
            // Manage duration
            if (ch.duration_samples > 0) {
                ch.duration_samples--;
                if (ch.duration_samples == 0) {
                    ch.active = false;
                }
            }
        }
        
        // Clamp and convert to 16-bit PCM (interleaved stereo)
        mixed_sample = std::clamp(mixed_sample, -1.0f, 1.0f);
        int16_t pcm_val = static_cast<int16_t>(mixed_sample * 32767.0f);
        
        buffer[i * 2]     = pcm_val; // Left
        buffer[i * 2 + 1] = pcm_val; // Right
    }
}

} // namespace ooey_station::console
