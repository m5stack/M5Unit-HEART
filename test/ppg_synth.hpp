/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Synthetic PPG generator for native tests
*/
#ifndef M5_UNIT_HEART_TEST_PPG_SYNTH_HPP
#define M5_UNIT_HEART_TEST_PPG_SYNTH_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <heart/ppg_types.hpp>

namespace ppg_synth {

struct Config {
    float sampling_rate_hz{100.0f};
    float bpm{72.0f};
    float ratio{0.6f};                    // R: red pulsatile ratio / IR pulsatile ratio
    float dc[3]{2.0e6f, 1.5e6f, 1.0e6f};  // Green, Red, IR
    float perfusion{0.02f};               // Peak-to-peak / DC of Green and IR
    float noise{0.0f};                    // Gaussian noise, standard deviation / DC
    float wander{0.0f};                   // Baseline wander (0.25 Hz) amplitude / DC
    uint8_t channels{0x07};               // Generated channels (channel_bit)
    uint32_t drop_every{0};               // Every N-th sample lacks drop_channels (0: never)
    uint8_t drop_channels{0};             // Channels dropped (channel_bit)
    uint32_t seed{1};                     // Noise seed
};

class Generator {
public:
    explicit Generator(const Config& cfg) : _cfg(cfg), _rng{cfg.seed ? cfg.seed : 1U}
    {
        float lo{1e9f}, hi{-1e9f};
        for (int i = 0; i < 1000; ++i) {
            const float v = raw_shape(i / 1000.0f);
            lo            = std::min(lo, v);
            hi            = std::max(hi, v);
        }
        _shape_min   = lo;
        _shape_range = hi - lo;
    }

    //! Change the heart rate keeping the phase continuous
    void setBpm(const float bpm)
    {
        _cfg.bpm = bpm;
    }
    //! Change the perfusion (peak-to-peak / DC of Green and IR)
    void setPerfusion(const float perfusion)
    {
        _cfg.perfusion = perfusion;
    }
    //! Elapsed time (s)
    float time() const
    {
        return _index / _cfg.sampling_rate_hz;
    }

    m5::heart::PPGSample next()
    {
        const float pi{3.14159265f};
        const float t             = time();
        const float shape         = (raw_shape(_phase) - _shape_min) / _shape_range;  // 0...1, 1 at systole
        const float base          = 1.0f + _cfg.wander * std::sin(2.0f * pi * 0.25f * t);
        const float modulation[3] = {_cfg.perfusion, _cfg.perfusion * _cfg.ratio, _cfg.perfusion};

        m5::heart::PPGSample s{};
        const bool drop = _cfg.drop_every && ((_index + 1) % _cfg.drop_every) == 0;
        for (uint8_t c = 0; c < 3; ++c) {
            const uint8_t bit = static_cast<uint8_t>(1U << c);
            if (!(_cfg.channels & bit) || (drop && (_cfg.drop_channels & bit))) {
                continue;
            }
            // Reflective PPG: the value falls in systole
            const float v = _cfg.dc[c] * (base - modulation[c] * shape) + _cfg.noise * _cfg.dc[c] * gaussian();
            s.value[c]    = v;
            s.valid |= bit;
        }
        ++_index;
        _phase += _cfg.bpm / 60.0f / _cfg.sampling_rate_hz;
        if (_phase >= 1.0f) {
            _phase -= 1.0f;
        }
        return s;
    }

private:
    // Systolic wave and a smaller diastolic wave within one beat (phase 0...1)
    static float raw_shape(const float phase)
    {
        const float s = (phase - 0.2f) / 0.08f;
        const float d = (phase - 0.5f) / 0.12f;
        return std::exp(-s * s) + 0.35f * std::exp(-d * d);
    }
    uint32_t xorshift()
    {
        _rng ^= _rng << 13;
        _rng ^= _rng >> 17;
        _rng ^= _rng << 5;
        return _rng;
    }
    float uniform()
    {
        return (xorshift() + 1.0f) / 4294967297.0f;  // (0, 1)
    }
    float gaussian()
    {
        const float pi{3.14159265f};
        return std::sqrt(-2.0f * std::log(uniform())) * std::cos(2.0f * pi * uniform());
    }

    Config _cfg{};
    uint32_t _rng{};
    uint32_t _index{};
    float _phase{};
    float _shape_min{}, _shape_range{1.0f};
};

}  // namespace ppg_synth
#endif
