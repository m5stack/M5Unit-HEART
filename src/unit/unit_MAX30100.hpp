/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_MAX30100.hpp
  @brief MAX30100 Unit for M5UnitUnified
*/
#ifndef M5_UNIT_HEART_UNIT_MAX30100_HPP
#define M5_UNIT_HEART_UNIT_MAX30100_HPP

#include <M5UnitComponent.hpp>
#include <m5_utility/stl/extension.hpp>
#include <m5_utility/container/circular_buffer.hpp>
#include <limits>  // NaN

namespace m5 {
namespace unit {
/*!
  @namespace max30100
  @brief For MAX30100
 */
namespace max30100 {
/*!
  @enum Mode
  @brief Operation mode
 */
enum class Mode : uint8_t {
    None,           //!< None
    HROnly = 0x02,  //!< HR only enabled
    SPO2,           //!< SPO2 and HR enabled
};

/*!
  @struct ModeConfiguration
  @brief Accessor for ModeConfiguration
*/
struct ModeConfiguration {
    ///@name Getter
    ///@{
    bool shdn() const
    {
        return value & (1U << 7);
    }  //!< @brief Shutdown Control (SHDN)
    bool reset() const
    {
        return value & (1U << 6);
    }  //!< @brief Reset control
    bool temperature() const
    {
        return value & (1U << 3);
    }  //!< @brief Temperature enable
    Mode mode() const
    {
        return static_cast<Mode>(value & 0x07);
    }  //!< @brief Mode control
    ///@}

    ///@name Setter
    ///@{
    void shdn(const bool b)
    {
        value = (value & ~(1U << 7)) | ((b ? 1 : 0) << 7);
    }  //!< @brief Shutdown Control (SHDN)
    void reset(const bool b)
    {
        value = (value & ~(1U << 6)) | ((b ? 1 : 0) << 6);
    }  //!< @brief Reset control
    void temperature(const bool b)
    {
        value = (value & ~(1U << 3)) | ((b ? 1 : 0) << 3);
    }  //!< @brief Temperature enable
    void mode(const Mode m)
    {
        value = (value & ~0x07) | (m5::stl::to_underlying(m) & 0x07);
    }  //!< @brief Mode control
    ///@}

    uint8_t value{};
};

/*!
  @enum Sample
  @brief Sample rate for pulse
  @details Unit is the number of sample per second
 */
enum class Sample : uint8_t {
    Rate50,    //!< 50 sps
    Rate100,   //!< 100 sps
    Rate167,   //!< 167 sps
    Rate200,   //!< 200 sps
    Rate400,   //!< 400 sps
    Rate600,   //!< 600 sps
    Rate800,   //!< 800 sps
    Rate1000,  //!< 1000 sps
};

/*!
  @enum LedPulseWidth
  @brief  LED pulse width (the IR and RED have the same pulse width)
*/
enum class LedPulseWidth {
    PW200,   //!< 200 us (ADC 13 bits)
    PW400,   //!< 400 us (ADC 14 bits)
    PW800,   //!< 800 us (ADC 15 bits)
    PW1600,  //!< 1600 us (ADC 16 bits)
};

/*!
  @struct SpO2Configuration
  @brief Accessor for SpO2Configuration
  @warning Note that there are different combinations that can be set depending
  on the mode

  - Mode:SPO2
  |Sample\PulseWidth|200 |400 |800 |1600|
  |---|---|---|---|---|
  |  50|o|o|o|o|
  | 100|o|o|o|o|
  | 167|o|o|o|x|
  | 200|o|o|o|x|
  | 400|o|o|x|x|
  | 600|o|x|x|x|
  | 800|o|x|x|x|
  |1000|o|x|x|x|
  - Mode:HROnly
  |Sample\PulseWidth|200 |400 |800 |1600|
  |---|---|---|---|---|
  |  50|o|o|o|o|
  | 100|o|o|o|o|
  | 167|o|o|o|x|
  | 200|o|o|o|x|
  | 400|o|o|x|x|
  | 600|o|o|x|x|
  | 800|o|o|x|x|
  |1000|o|o|x|x|
*/
struct SpO2Configuration {
    ///@name Getter
    ///@{
    bool highResolution() const
    {  // for SpO2
        return value & (1U << 6);
    }
    Sample sampleRate() const
    {
        return static_cast<Sample>((value >> 2) & 0x07);
    }
    LedPulseWidth ledPulseWidth() const
    {
        return static_cast<LedPulseWidth>(value & 0x03);
    }
    ///@}

    ///@name Setter
    ///@{
    void highResolution(const bool b)
    {  // for SpO2
        value = (value & ~(1U << 6)) | ((b ? 1 : 0) << 6);
    }
    void sampleRate(const Sample rate)
    {
        value = (value & ~(0x07 << 2)) | ((m5::stl::to_underlying(rate) & 0x07) << 2);
    }
    void ledPulseWidth(const LedPulseWidth width)
    {
        value = (value & ~0x03) | (m5::stl::to_underlying(width) & 0x03);
    }
    ///@}

    uint8_t value{};
};

/*!
  @enum CurrentControl
  @brief Current level for Led
 */
enum class CurrentControl {
    mA0_0,   //!< @brief 0,0 mA
    mA4_4,   //!< @brief 4,4 mA
    mA7_6,   //!< @brief 7.6 mA
    mA11_0,  //!< @brief 11.0 mA
    mA14_2,  //!< @brief 14.2 mA
    mA17_4,  //!< @brief 17.4 mA
    mA20_8,  //!< @brief 20,8 mA
    mA24_0,  //!< @brief 24.0 mA
    mA27_1,  //!< @brief 27.1 mA
    mA30_6,  //!< @brief 30.6 mA
    mA33_8,  //!< @brief 33.8 mA
    mA37_0,  //!< @brief 37.0 mA
    mA40_2,  //!< @brief 40.2 mA
    mA43_6,  //!< @brief 43.6 mA
    mA46_8,  //!< @brief 46.8 mA
    mA50_0,  //!< @brief 50.0 mA
};

/*!
  @struct LedConfiguration
  @brief Accessor for LedConfiguration
 */
struct LedConfiguration {
    ///@name Getter
    ///@{
    CurrentControl red() const
    {
        return static_cast<CurrentControl>((value >> 4) & 0x0F);
    }
    CurrentControl ir() const
    {
        return static_cast<CurrentControl>(value & 0x0F);
    }
    ///@}
    ///@name Setter
    ///@{
    void red(const CurrentControl cc)
    {
        value = (value & ~(0x0F << 4)) | ((m5::stl::to_underlying(cc) & 0x0F) << 4);
    }
    void ir(const CurrentControl cc)
    {
        value = (value & ~0x0F) | (m5::stl::to_underlying(cc) & 0x0F);
    }
    ///@}
    uint8_t value{};
};

//! @brief FIFO depth
constexpr uint8_t MAX_FIFO_DEPTH{16};

/*!
  @struct Data
  @brief Measurement data group
 */
struct Data {
    std::array<uint8_t, 4> raw{};
    uint16_t ir() const;   //!< IR
    uint16_t red() const;  //!< RED
};

/*!
  @struct TemperatureData
  @brief Measurement data group for temperature
 */
struct TemperatureData {
    std::array<uint8_t, 2> raw{};
    //! temperature (Celsius)
    inline float temperature() const
    {
        return celsius();
    }
    float celsius() const;     //!< temperature (Celsius)
    float fahrenheit() const;  //!< temperature (Fahrenheit)
};

}  // namespace max30100

/*!
  @class UnitMAX30100
  @brief Pulse oximetry and heart-rate sensor
  @note The only single measurement is temperature; other data is constantly
  measured and stored
*/
class UnitMAX30100 : public Component, public PeriodicMeasurementAdapter<UnitMAX30100, max30100::Data> {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitMAX30100, 0x57);

public:
    /*!
      @struct config_t
      @brief Settings for begin
      @warning Note that some combinations of sample_rate and pulse_width are invalid. See also datasheet
     */
    struct config_t {
        //! Start periodic measurement on begin?
        bool start_periodic{true};
        //! Operating mode if start on begin
        max30100::Mode mode{max30100::Mode::SPO2};
        //! Sample rate if start on begin
        max30100::Sample sample_rate{m5::unit::max30100::Sample::Rate100};
        //! Led pulse width if start on begin
        max30100::LedPulseWidth pulse_width{m5::unit::max30100::LedPulseWidth::PW1600};
        //!  Led current for IR if start on begin
        m5::unit::max30100::CurrentControl ir_current{m5::unit::max30100::CurrentControl::mA27_1};
        //! The SpO2 ADC resolution if start on begin (only SpO2)
        bool high_resolution{true};
        //! Led current for Red if start on begin (only SpO2)
        m5::unit::max30100::CurrentControl red_current{m5::unit::max30100::CurrentControl::mA27_1};
    };

    explicit UnitMAX30100(const uint8_t addr = DEFAULT_ADDRESS)
        : Component(addr), _data{new m5::container::CircularBuffer<max30100::Data>(max30100::MAX_FIFO_DEPTH)}
    {
        auto ccfg        = component_config();
        ccfg.clock       = 400 * 1000U;
        ccfg.stored_size = max30100::MAX_FIFO_DEPTH;
        component_config(ccfg);
    }
    virtual ~UnitMAX30100()
    {
    }

    virtual bool begin() override;
    virtual void update(const bool force = false) override;

    ///@name Settings for begin
    ///@{
    /*! @brief Gets the configration */
    inline config_t config()
    {
        return _cfg;
    }
    //! @brief Set the configration
    inline void config(const config_t& cfg)
    {
        _cfg = cfg;
    }
    ///@}

    ///@name Measurement data by periodic
    ///@{
    //! @brief Oldest IR
    inline uint16_t ir() const
    {
        return !empty() ? oldest().ir() : 0;
    }
    //! @brief Oldest Red
    inline uint16_t red() const
    {
        return !empty() ? oldest().red() : 0;
    }
    /*!
      @brief Number of data last retrieved
      @note The number of data retrieved by the latest update, not all data
      accumulated
      @sa available()
    */
    inline uint8_t retrived() const
    {
        return _retrived;
    }
    /*!
      @brief The number of samples lost
      @note It saturates at 15
     */
    inline uint8_t overflow() const
    {
        return _overflow;
    }
    ///@}

    ///@name Periodic measurement
    ///@{
    /*!
      @brief Start periodic measurement in the current settings
      @return True if successful
    */
    inline bool startPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitMAX30100, max30100::Data>::startPeriodicMeasurement();
    }
    /*!
      @brief Start periodic measurement
      @param mode Operation mode
      @param sample_rate  Sample rate
      @param pulse_width Pulse width
      @param ir_current IR Led control
      @param high_resolution (only SpO2)
      @param red_current RED Led control (only SpO2)
      @return True if successful
      @warning Note that some combinations of sample_rate and pulse_width are invalid. See also datasheet
    */
    inline bool startPeriodicMeasurement(const max30100::Mode mode, const max30100::Sample sample_rate,
                                         const max30100::LedPulseWidth pulse_width,
                                         const max30100::CurrentControl ir_current, const bool high_resolution = false,
                                         const max30100::CurrentControl red_current = max30100::CurrentControl::mA0_0)
    {
        return PeriodicMeasurementAdapter<UnitMAX30100, max30100::Data>::startPeriodicMeasurement(
            mode, sample_rate, pulse_width, ir_current, high_resolution, red_current);
    }
    /*!
      @brief Stop periodic measurement
      @return True if successful
    */
    inline bool stopPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitMAX30100, max30100::Data>::stopPeriodicMeasurement();
    }

    ///@warning Note that there are different combinations that can be set
    /// depending on the mode See also SpO2Configuration
    ///@name Mode Configuration
    ///@{
    /*!
      @brief Read Mode configuration
      @param[out] mc ModeConfigration
      @return True if successful
     */
    bool readModeConfiguration(max30100::ModeConfiguration& mc);
    /*!
      @brief Write Mode configuration
      @param mc ModeConfigration
      @return True if successful
    */
    bool writeModeConfiguration(const max30100::ModeConfiguration mc);
    //! @brief Write Mode
    bool writeMode(const max30100::Mode mode);
    //! @brief Write power save mode to enable
    bool writePowerSaveEnable()
    {
        return enable_power_save(true);
    }
    //! @brief Write power save mode to disable
    bool writePowerSaveDisable()
    {
        return enable_power_save(false);
    }
    ///@}

    ///@warning Note that there are different combinations that can be set depending on the mode See also
    /// SpO2Configuration
    ///@name SpO2 Configuration
    ///@{
    /*!
      @brief Read SpO2 configrartion
      @param[out] sc SpO2Configration
      @return True if successful
    */
    bool readSpO2Configuration(max30100::SpO2Configuration& sc);
    /*!
      @brief Write SpO2 configrartion
      @param sc SpO2Configration
      @return True if successful
    */
    bool writeSpO2Configuration(const max30100::SpO2Configuration sc);
    //! @brief Write sample rate
    bool writeSampleRate(const max30100::Sample rate);
    //! @brief Write LED pulse width
    bool writeLedPulseWidth(const max30100::LedPulseWidth width);
    //! @brief Writee SpO2 high resolution mode to enable
    inline bool writeHighResolutionEnable()
    {
        return enable_high_resolution(true);
    }
    //! @brief Write SpO2 high resolution mode to disable
    inline bool writeHighResolutionDisable()
    {
        return enable_high_resolution(false);
    }
    ///@}

    ///@warning In the heart-rate only mode, the red LED is inactive.
    /// and only the IR LED is used to capture optical data and determine
    /// the heart rate.
    ///@name LED Configuration
    ///@{
    /*!
      @brief Read Led configrartion
      @param[out] lc LedConfigration
      @return True if successful
    */
    bool readLedConfiguration(max30100::LedConfiguration& lc);
    /*!
      @brief Write Led configrartion
      @param lc LedConfigration
      @return True if successful
    */
    bool writeLedConfiguration(const max30100::LedConfiguration lc);
    //! @brief Write IR/RED current
    bool writeLedCurrent(const max30100::CurrentControl ir, const max30100::CurrentControl red);
    ///@}

    ///@note The temperature sensor data can be used to compensate the SpO2
    /// error with ambient temperature changes
    ///@name Measurement temperature
    ///@{
    /*!
      @brief Measure tempeature single shot
      @param[out] temp Temperature(Celsius)
      @return True if successful
      @warning Blocking until measured about 29 ms
     */
    bool measureTemperatureSingleshot(max30100::TemperatureData& td);
    ///@}

    /*!
      @brief Reset FIFO buffer
      @return True if successful
    */
    bool resetFIFO();
    /*!
      @brief Reset
      @return True if successful
      @warning Blocked until the reset process is completed
     */
    bool reset();

protected:
    bool start_periodic_measurement();
    bool start_periodic_measurement(const max30100::Mode mode, const max30100::Sample sample_rate,
                                    const max30100::LedPulseWidth pulse_width,
                                    const max30100::CurrentControl ir_current, const bool high_resolution,
                                    const max30100::CurrentControl red_current);
    bool stop_periodic_measurement();

    bool read_FIFO();
    bool read_measurement_temperature(max30100::TemperatureData& td);

    M5_UNIT_COMPONENT_PERIODIC_MEASUREMENT_ADAPTER_HPP_BUILDER(UnitMAX30100, max30100::Data);

    bool read_mode_configration(uint8_t& c);
    bool write_mode_configration(const uint8_t c);
    bool enable_power_save(const bool enabled);
    bool read_spo2_configration(uint8_t& c);
    bool write_spo2_configration(const uint8_t c);
    bool enable_high_resolution(const bool enabled);
    bool read_led_configration(uint8_t& c);
    bool write_led_configration(const uint8_t c);

    bool read_register(const uint8_t reg, uint8_t* buf, const size_t len);
    bool read_register8(const uint8_t reg, uint8_t& v);

protected:
    max30100::Mode _mode{max30100::Mode::None};
    uint8_t _retrived{}, _overflow{};
    std::unique_ptr<m5::container::CircularBuffer<max30100::Data>> _data{};

    config_t _cfg{};
};

///@cond
namespace max30100 {
namespace command {
// STATUS
constexpr uint8_t READ_INTERRUPT_STATUS{0x00};
constexpr uint8_t INTERRUPT_ENABLE{0x01};
// FIFO
constexpr uint8_t FIFO_WRITE_POINTER{0x02};
constexpr uint8_t FIFO_OVERFLOW_COUNTER{0x03};
constexpr uint8_t FIFO_READ_POINTER{0x04};
// Note that FIFO_DATA_REGISTER cannot be burst read.
constexpr uint8_t FIFO_DATA_REGISTER{0x05};
// CONFIGURATION
constexpr uint8_t MODE_CONFIGURATION{0x06};
constexpr uint8_t SPO2_CONFIGURATION{0x07};
constexpr uint8_t LED_CONFIGURATION{0x09};
// TEMPERATURE
constexpr uint8_t TEMP_INTEGER{0x16};
constexpr uint8_t TEMP_FRACTION{0x17};
// PART ID
constexpr uint8_t READ_REVISION_ID{0xFE};
constexpr uint8_t PART_ID{0xFF};

}  // namespace command
}  // namespace max30100
///@endcond

}  // namespace unit
}  // namespace m5

#endif
