/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_GH3038.hpp
  @brief GH3038 Unit for M5UnitUnified
*/
#ifndef M5_UNIT_HEART_UNIT_GH3038_HPP
#define M5_UNIT_HEART_UNIT_GH3038_HPP

#include <memory>

#include <M5UnitComponent.hpp>
#include <m5_utility/container/circular_buffer.hpp>

#include "../heart/ppg_data_source.hpp"
#include "gh3038_core.h"
#include "gh3038_types.hpp"

namespace m5 {
namespace unit {

/*!
  @class m5::unit::UnitGH3038
  @brief PPG analog front end (raw data of Green, Red and IR)
  @note The chip control is done by the precompiled core (libm5gh3038core.a)
  @warning The chip FIFO holds 255 words. In Mode::All at 100 Hz it overflows in about 0.85 s (0.42 s at 200 Hz), so
  update() must be called more often than that. INT only tells when to read; it does not prevent the overflow.
 */
class UnitGH3038 : public Component,
                   public PeriodicMeasurementAdapter<UnitGH3038, gh3038::Data>,
                   public m5::heart::PPGDataSource {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitGH3038, 0x14);

public:
    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        //! Start periodic measurement on begin?
        bool start_periodic{true};
        //! Measurement mode if start on begin
        gh3038::Mode mode{gh3038::Mode::All};
        //! Sampling rate if start on begin
        gh3038::SamplingRate rate{gh3038::SamplingRate::Rate100};
        //! TIA gain level for Green
        gh3038::Gain green_gain{gh3038::Gain::Level3};
        //! TIA gain level for Red
        gh3038::Gain red_gain{gh3038::Gain::Level1};
        //! TIA gain level for IR
        gh3038::Gain ir_gain{gh3038::Gain::Level1};
        //! LED current code for Green (0...255)
        uint8_t green_current{64};
        //! LED current code for Red (0...255)
        uint8_t red_current{128};
        //! LED current code for IR (0...255)
        uint8_t ir_current{128};
        //! RST pin (-1: not driven, soft reset is used and RST must be kept high externally)
        int8_t rst_pin{-1};
        //! INT pin (-1: polling)
        int8_t int_pin{-1};
    };

    /*!
      @brief Constructor
      @param addr I2C address
     */
    explicit UnitGH3038(const uint8_t addr = DEFAULT_ADDRESS)
        : Component(addr), _data{new m5::container::CircularBuffer<gh3038::Data>(m5_gh3038_max_samples())}
    {
        auto ccfg        = component_config();
        ccfg.clock       = 400 * 1000U;
        ccfg.stored_size = m5_gh3038_max_samples();
        component_config(ccfg);
    }
    virtual ~UnitGH3038();

    //! @brief Begin the unit
    virtual bool begin() override;
    //! @brief Update the unit
    virtual void update(const bool force = false) override;

    ///@name Settings for begin
    ///@{
    //! @brief Gets the configuration
    inline config_t config() const
    {
        return _cfg;
    }
    //! @brief Set the configuration
    inline void config(const config_t& cfg)
    {
        _cfg = cfg;
    }
    ///@}

    ///@name Measurement data by periodic
    ///@{
    //! @brief Oldest Green (0 if empty or not measured)
    inline int32_t green() const
    {
        return !empty() ? oldest().green() : 0;
    }
    //! @brief Oldest Red (0 if empty or not measured)
    inline int32_t red() const
    {
        return !empty() ? oldest().red() : 0;
    }
    //! @brief Oldest IR (0 if empty or not measured)
    inline int32_t ir() const
    {
        return !empty() ? oldest().ir() : 0;
    }
    //! @brief Number of data retrieved by the latest update
    inline uint16_t retrieved() const
    {
        return _retrieved;
    }
    //! @brief Did the FIFO overflow before the latest update?
    inline bool overflowed() const
    {
        return _overflowed;
    }
    ///@}

    //! @brief Current measurement mode
    inline gh3038::Mode mode() const
    {
        return _mode;
    }
    //! @brief Current sampling rate
    inline gh3038::SamplingRate samplingRate() const
    {
        return _rate;
    }
    //! @brief Is the INT interrupt in use?
    inline bool isInterruptEnabled() const
    {
        return _int_enabled;
    }

    ///@name Periodic measurement
    ///@{
    /*!
      @brief Start periodic measurement in the current settings
      @return True if successful
     */
    inline bool startPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitGH3038, gh3038::Data>::startPeriodicMeasurement();
    }
    /*!
      @brief Start periodic measurement
      @param mode Measurement mode
      @param rate Sampling rate
      @return True if successful
     */
    inline bool startPeriodicMeasurement(const gh3038::Mode mode, const gh3038::SamplingRate rate)
    {
        return PeriodicMeasurementAdapter<UnitGH3038, gh3038::Data>::startPeriodicMeasurement(mode, rate);
    }
    /*!
      @brief Stop periodic measurement
      @return True if successful
     */
    inline bool stopPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitGH3038, gh3038::Data>::stopPeriodicMeasurement();
    }
    ///@}

    ///@name Settings
    ///@{
    /*!
      @brief Read the TIA gain level
      @param[out] gain Gain level
      @param color LED color
      @return True if successful
     */
    bool readGain(gh3038::Gain& gain, const gh3038::Color color);
    /*!
      @brief Write the TIA gain level
      @param color LED color
      @param gain Gain level
      @return True if successful
      @note Can be changed during periodic measurement
      @note Samples already in the chip FIFO when the gain is changed are tagged with the new gain
     */
    bool writeGain(const gh3038::Color color, const gh3038::Gain gain);
    /*!
      @brief Read the LED current code
      @param[out] code Current code (0...255)
      @param color LED color
      @return True if successful
     */
    bool readLEDCurrent(uint8_t& code, const gh3038::Color color);
    /*!
      @brief Write the LED current code
      @param color LED color
      @param code Current code (0...255)
      @return True if successful
      @note Can be changed during periodic measurement
     */
    bool writeLEDCurrent(const gh3038::Color color, const uint8_t code);
    ///@}

    ///@name PPGDataSource
    ///@{
    /*!
      @brief What this unit provides
      @return HR: Green, SpO2: Red and IR, All: Green, Red and IR. Unit is PicoAmpere (converted with the gain of
      each sample). The sampling rate is 0 if not in periodic measurement
     */
    virtual m5::heart::PPGSpec ppgSpec() const override;
    /*!
      @brief Pop the oldest sample not yet read by readPPG()
      @param[out] out Sample (pA)
      @return True if a sample was stored in out
      @note It does not change available(), oldest() or discard(). Samples removed by discard() or flush() before
      readPPG() reads them are skipped, so call it (or PulseController::update()) before discarding
     */
    virtual bool readPPG(m5::heart::PPGSample& out) override;
    //! @brief Did the latest update detect a FIFO overflow?
    virtual bool ppgOverflowed() const override
    {
        return _overflowed;
    }
    ///@}

    /*!
      @brief Soft reset
      @return True if successful
      @note Periodic measurement is stopped
     */
    bool softReset();

protected:
    bool start_periodic_measurement();
    bool start_periodic_measurement(const gh3038::Mode mode, const gh3038::SamplingRate rate);
    bool stop_periodic_measurement();

    bool read_fifo();
    bool read_fifo_level(uint16_t& words);
    bool setup_rst_pin();
    bool setup_interrupt();
    void remove_interrupt();
    bool interrupt_asserted() const;
    static void isr_handler(void* arg);

    // Callbacks for the core
    static bool io_write(void* user, const uint16_t reg, const uint8_t* data, const uint32_t len);
    static bool io_read(void* user, const uint16_t reg, uint8_t* rx, const uint32_t len);
    static void io_delay_us(void* user, const uint32_t us);
    static bool io_set_rst(void* user, const int level);
    static void on_sample(void* user, const m5_gh3038_sample_t* sample);

    M5_UNIT_COMPONENT_PERIODIC_MEASUREMENT_ADAPTER_HPP_BUILDER(UnitGH3038, gh3038::Data);

protected:
    std::unique_ptr<m5::container::CircularBuffer<gh3038::Data>> _data{};
    config_t _cfg{};
    m5_gh3038_state_t _core{};
    gh3038::Mode _mode{gh3038::Mode::All};
    gh3038::SamplingRate _rate{gh3038::SamplingRate::Rate100};
    uint16_t _retrieved{};
    bool _overflowed{};
    bool _int_enabled{};
    int _int_idle_level{};
    int8_t _int_pin{-1};
    volatile bool _irq_pending{};
    uint32_t _ppg_pushed{}, _ppg_consumed{};  // readPPG() pop counters (independent of oldest()/discard())
};

}  // namespace unit
}  // namespace m5
#endif
