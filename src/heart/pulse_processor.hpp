/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file pulse_processor.hpp
  @brief Heart rate, SpO2 and perfusion index from PPG samples
*/
#ifndef M5_UNIT_HEART_HEART_PULSE_PROCESSOR_HPP
#define M5_UNIT_HEART_HEART_PULSE_PROCESSOR_HPP

#include <cstdint>

#include "ppg_types.hpp"
#include "filter/bandpass.hpp"
#include "beat_detector.hpp"
#include "spo2_estimator.hpp"

namespace m5 {
namespace heart {

/*!
  @class PulseProcessor
  @brief Chip-independent PPG processing (push one sample at a time)
  @details
  - Heart rate: 0.5-4 Hz band-pass and adaptive-threshold peak detection on the heart rate channel
  - SpO2: beat-synchronous ratio of ratios of Red and IR (provisional conversion, not calibrated)
  - Perfusion index: pulsatile peak-to-peak of the latest beat / DC x 100 (%)
  The values do not depend on the unit or the scale of the input.
 */
class PulseProcessor {
public:
    /*!
      @struct config_t
      @brief Processing settings
     */
    struct config_t {
        //! Heart rate channel (-1: automatic (Green, IR, Red in this order), 0...2: Channel)
        int8_t hr_channel{-1};
        //! Calculate SpO2 (only when both Red and IR are available)
        bool spo2{true};
    };

    /*!
      @brief Constructor with the default settings
      @param sampling_rate_hz Sampling rate (Hz). push() does nothing if not positive
      @param channels Available channels (channel_bit)
     */
    PulseProcessor(const float sampling_rate_hz, const uint8_t channels);
    /*!
      @brief Constructor
      @param sampling_rate_hz Sampling rate (Hz). push() does nothing if not positive
      @param channels Available channels (channel_bit)
      @param cfg Settings
     */
    PulseProcessor(const float sampling_rate_hz, const uint8_t channels, const config_t& cfg);

    /*!
      @brief Process a sample
      @param s Sample
      @note A sample without a finite value of the heart rate channel is ignored
     */
    void push(const PPGSample& s);
    //! @brief Discard all states and histories (the settings are kept)
    void reset();

    ///@name Results
    ///@{
    //! @brief Is the heart rate valid?
    bool hasBpm() const;
    //! @brief Heart rate (bpm), NaN if not valid
    float bpm() const;
    //! @brief Is SpO2 valid?
    bool hasSpO2() const;
    /*!
      @brief SpO2 (%), NaN if not valid
      @warning Provisional conversion (110 - 25R), not calibrated
     */
    float spo2() const;
    //! @brief Did the latest push() confirm a valid beat?
    inline bool isBeat() const
    {
        return _beat;
    }
    //! @brief Perfusion index of the latest valid beat (%), NaN if none
    inline float perfusionIndex() const
    {
        return _perfusion_index;
    }
    /*!
      @brief Pulsatile component of the heart rate channel at the latest push()
      @note Band-passed and inverted (systolic peak positive), in the unit of the input. For plotting
     */
    inline float pulseWave() const
    {
        return _wave;
    }
    ///@}

    ///@name Settings
    ///@{
    //! @brief Sampling rate (Hz)
    inline float samplingRate() const
    {
        return _rate;
    }
    //! @brief Available channels (channel_bit)
    inline uint8_t channels() const
    {
        return _channels;
    }
    //! @brief Heart rate channel in use (Channel value, INVALID_CHANNEL if none)
    inline uint8_t heartRateChannel() const
    {
        return _hr;
    }
    //! @brief Is SpO2 calculated?
    inline bool isSpO2Enabled() const
    {
        return _spo2_enabled;
    }
    //! @brief Settings
    inline config_t config() const
    {
        return _cfg;
    }
    ///@}

private:
    struct Accumulator {
        float ac_max{}, ac_min{}, raw_sum{};
        uint32_t count{};
        void clear();
        void add(const float ac, const float raw);
        void merge(const Accumulator& o);
        float range() const;
        float mean() const;
    };
    void accumulate(const uint8_t events, const float* ac, const float* raw, const bool spo2_sample);
    void finish_beat(const bool valid);

private:
    float _rate{};
    uint8_t _channels{};
    config_t _cfg{};
    uint8_t _hr{INVALID_CHANNEL};
    bool _spo2_enabled{};

    float _dc_alpha{};
    uint32_t _settle{};
    uint32_t _count{};
    bool _primed[NUMBER_OF_CHANNELS]{};
    float _dc[NUMBER_OF_CHANNELS]{};
    BandPass _bandpass[NUMBER_OF_CHANNELS]{};
    BeatDetector _detector{};
    SpO2Estimator _estimator{};

    Accumulator _interval[NUMBER_OF_CHANNELS]{};  // Latest trough to the trough candidate
    Accumulator _pending[NUMBER_OF_CHANNELS]{};   // Trough candidate to now
    uint8_t _peaks{};

    bool _beat{};
    float _perfusion_index{};
    float _wave{};
};

}  // namespace heart
}  // namespace m5
#endif
