/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file beat_detector.hpp
  @brief Beat detection on the pulsatile PPG component
*/
#ifndef M5_UNIT_HEART_HEART_BEAT_DETECTOR_HPP
#define M5_UNIT_HEART_HEART_BEAT_DETECTOR_HPP

#include <cstdint>

#include "recent_values.hpp"

namespace m5 {
namespace heart {

/*!
  @class BeatDetector
  @brief Adaptive-threshold peak detection with refractory period and RR checks
  @details Input is the pulsatile component with the systolic peak positive. process() returns event bits:
  - TROUGH_CANDIDATE: the sample is the lowest since the latest peak (candidate of the trough)
  - PEAK: a peak was confirmed (the latest trough candidate is final)
  - BEAT: the confirmed peak made a valid RR interval
  - EXPIRED: no valid beat for the timeout, the RR history was discarded
  - RESEEDED: the rhythm changed, the RR history was replaced with the recent intervals (with BEAT)
 */
class BeatDetector {
public:
    ///@name Event bits returned by process()
    ///@{
    static constexpr uint8_t TROUGH_CANDIDATE{0x01};  //!< The sample is the new trough candidate
    static constexpr uint8_t PEAK{0x02};              //!< A peak was confirmed
    static constexpr uint8_t BEAT{0x04};              //!< The peak made a valid RR interval
    static constexpr uint8_t EXPIRED{0x08};           //!< No valid beat for the timeout
    static constexpr uint8_t RESEEDED{0x10};          //!< The RR history was replaced (rhythm changed)
    ///@}

    //! @brief Inactive detector (process() does nothing)
    BeatDetector() = default;
    /*!
      @brief Constructor
      @param sampling_rate_hz Sampling rate (Hz)
     */
    explicit BeatDetector(const float sampling_rate_hz);

    //! @brief Clear all states
    void reset();

    /*!
      @brief Process a sample
      @param ac Pulsatile component (systolic peak positive)
      @return Event bits
     */
    uint8_t process(const float ac);

    //! @brief Number of valid RR intervals in the history
    inline size_t rrCount() const
    {
        return _rr.size();
    }
    //! @brief Number of consecutive valid beats up to the latest peak
    inline uint32_t consecutiveBeats() const
    {
        return _consecutive;
    }
    //! @brief Was a valid beat detected within the timeout?
    inline bool hasRecentBeat() const
    {
        return _has_beat;
    }
    //! @brief Heart rate from the median of the recent RR intervals (bpm), NaN if no RR
    float bpm() const;

private:
    float candidate_offset() const;
    bool accept_rr(const float rr);
    bool reject_rr(const float rr);

private:
    float _rate{};
    uint32_t _refractory{};
    float _decay{};
    float _quiet_decay{};
    uint32_t _quiet{};
    float _recent_decay{};
    uint32_t _timeout{};

    uint32_t _index{};
    float _previous{};
    float _envelope{};
    float _recent{};
    float _trough{};
    bool _above{};
    float _candidate{};
    float _candidate_before{};
    float _candidate_after{};
    bool _need_after{};
    uint32_t _candidate_index{};
    bool _has_peak{};
    uint32_t _peak_index{};
    float _peak_offset{};
    bool _has_beat{};
    uint32_t _beat_index{};
    uint32_t _consecutive{};

    RecentValues<5> _rr{};
    float _rejected[3]{};
    uint8_t _rejected_count{};
    bool _reseeded{};
};

}  // namespace heart
}  // namespace m5
#endif
