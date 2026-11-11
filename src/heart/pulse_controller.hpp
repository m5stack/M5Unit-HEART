/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file pulse_controller.hpp
  @brief Pulls samples from a PPG data source and processes them
*/
#ifndef M5_UNIT_HEART_HEART_PULSE_CONTROLLER_HPP
#define M5_UNIT_HEART_HEART_PULSE_CONTROLLER_HPP

#include <cstdint>

#include "ppg_data_source.hpp"
#include "pulse_processor.hpp"

namespace m5 {
namespace heart {

/*!
  @class PulseController
  @brief Connects one PPGDataSource to a PulseProcessor
  @details The sampling rate and the channels are taken from the source. Call update() after the unit is updated
  and before the unit data is discarded (readPPG() reads the data the unit keeps).
 */
class PulseController {
public:
    //! @brief Constructor with the default processing settings
    PulseController();
    /*!
      @brief Constructor
      @param cfg Processing settings
     */
    explicit PulseController(const PulseProcessor::config_t& cfg);

    /*!
      @brief Set the data source (replaces the previous one)
      @param src Data source (must outlive this controller)
      @return True if successful. False if the sampling rate of the source is not positive
     */
    bool add(PPGDataSource& src);

    /*!
      @brief Pull all unprocessed samples from the source and process them
      @return Number of processed samples (0 without a source)
      @note The processor is rebuilt when the sampling rate or the channels of the source change, and reset when the
      source reports a FIFO overflow together with new samples
     */
    uint32_t update();

    ///@name Results (same as PulseProcessor)
    ///@{
    //! @brief Is the heart rate valid?
    inline bool hasBpm() const
    {
        return _src && _processor.hasBpm();
    }
    //! @brief Heart rate (bpm), NaN if not valid
    inline float bpm() const
    {
        return _processor.bpm();
    }
    //! @brief Is SpO2 valid?
    inline bool hasSpO2() const
    {
        return _src && _processor.hasSpO2();
    }
    /*!
      @brief SpO2 (%), NaN if not valid
      @warning Provisional conversion (110 - 25R), not calibrated
     */
    inline float spo2() const
    {
        return _processor.spo2();
    }
    //! @brief Was a valid beat confirmed in the latest update()?
    inline bool isBeat() const
    {
        return _beat;
    }
    //! @brief Perfusion index of the latest valid beat (%), NaN if none
    inline float perfusionIndex() const
    {
        return _processor.perfusionIndex();
    }
    //! @brief Pulsatile component of the heart rate channel at the latest processed sample
    inline float pulseWave() const
    {
        return _processor.pulseWave();
    }
    ///@}

    //! @brief The processor (nullptr without a source)
    inline const PulseProcessor* processor() const
    {
        return _src ? &_processor : nullptr;
    }

private:
    void rebuild(const PPGSpec& spec);

private:
    PPGDataSource* _src{};
    PulseProcessor::config_t _cfg{};
    PulseProcessor _processor;
    bool _beat{};
};

}  // namespace heart
}  // namespace m5
#endif
