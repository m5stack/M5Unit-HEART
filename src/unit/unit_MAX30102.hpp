/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_MAX30102.hpp
  @brief MAX30102 Unit for M5UnitUnified
*/
#ifndef M5_UNIT_HEART_UNIT_MAX30102_HPP
#define M5_UNIT_HEART_UNIT_MAX30102_HPP

#include <M5UnitComponent.hpp>
#include <m5_utility/stl/extension.hpp>
#include <m5_utility/container/circular_buffer.hpp>
#include <limits>  // NaN

namespace m5 {
namespace unit {
/*!
  @namespace max30102
  @brief For MAX30102
 */
namespace max30102 {

/*!
  @enum Mode
  @brief Operation mode
 */
enum class Mode : uint8_t {
    None,             //!< None
    HROnly = 0x02,    //!< Heart Rate mode (Red only)
    SPO2,             //!< SpO2 mode (Red and IR)
    MultiLED = 0x07,  //!< Multi-LED mode (Red and IR)
};

/*!
  @enum SpO2ADCRange
  @brief SpO2 ADC Range Control
  @warning If the ambient environment is very bright or the sensor is exposed to strong light, setting a smaller value
  may cause the IR/RED to overflow
 */
enum class SpO2ADCRange : uint8_t {
    nA2048,   //!< LSB size  7.81 Full scale 2048
    nA4096,   //!< LSB size 15.63 Full scale 4096
    nA8192,   //!< LSB size 31.25 Full scale 8192
    nA16384,  //!< LSB size 62.5  Full scale 16384
};

/*!
  @enum Sampling
  @brief Sampling rate for pulse
  @details Unit is the number of sample per second
 */
enum class Sampling : uint8_t {
    Rate50,    //!< 50 sps
    Rate100,   //!< 100 sps
    Rate200,   //!< 200 sps
    Rate400,   //!< 400 sps
    Rate800,   //!< 800 sps
    Rate1000,  //!< 1000 sps
    Rate1600,  //!< 1600 sps
    Rate3200,  //!< 3200 sps
};

/*!
  @enum LEDPulseWidth
  @brief  LED pulse width (the IR and RED have the same pulse width)
*/
enum class LEDPulseWidth : uint8_t {
    PW69,   //!<  68.95 us (ADC 15 bits)
    PW118,  //!< 117.78 us (ADC 16 bits)
    PW215,  //!< 215.44 us (ADC 17 bits)
    PW411,  //!< 410.75 us (ADC 18 bits)
};

/*!
  @enum Slot
  @brief Multi-LED mode control
 */
enum class Slot : uint8_t {
    None,  //!< None (time slot is disabled)
    Red,   //!< LED1 (Red)
    IR,    //!< LED2 (IR)
};

/*!
  @enum FIFOSampling
  @brief Number of samples averaged per FIFO sample
 */
enum class FIFOSampling : uint8_t {
    Average1,   //!< 1 (no averaging)
    Average2,   //!< 2
    Average4,   //!< 4
    Average8,   //!< 8
    Average16,  //!< 16
    Average32,  //!< 32
    //    Average32, duplicated
    //    Average32, duplicated
};

struct ModeConfiguration;
struct SpO2Configuration;

constexpr uint8_t MAX_FIFO_DEPTH{32};  //!< @brief FIFO depth

/*!
  @struct Data
  @brief Measurement data group
 */
struct Data {
    std::array<uint8_t, 6> raw{};  // [0...2]:Red [3...5]:IR
    uint32_t mask{0x3FFFF};        // Valid bits based on ADC range
    //! IR value
    inline uint32_t ir() const
    {
        return mask & (((uint32_t)raw[3] << 16) | ((uint32_t)raw[4] << 8) | ((uint32_t)raw[5]));
    }
    //! Red value
    inline uint32_t red() const
    {
        return mask & (((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2]));
    }
};

/*!
  @struct TemperatureData
  @brief Measurement data group for temperature
 */
struct TemperatureData {
    std::array<uint8_t, 2> raw{0xFF, 0xFF};  // [0]:integer [1]:fraction
    //! Temperature (Celsius)
    inline float temperature() const
    {
        return celsius();
    }
    //!< Temperature (Celsius)
    inline float celsius() const
    {
        return (raw[0] != 0xFF) ? (int8_t)raw[0] + raw[1] * 0.0625f : std::numeric_limits<float>::quiet_NaN();
    }
    //!< temperature (Fahrenheit)
    inline float fahrenheit() const
    {
        return celsius() * 9.0f / 5.0f + 32.f;
    }
};

///@cond
namespace command {
// STATUS
constexpr uint8_t READ_INTERRUPT_STATUS_1{0x00};
constexpr uint8_t READ_INTERRUPT_STATUS_2{0x01};
constexpr uint8_t INTERRUPT_ENABLE_1{0x02};
constexpr uint8_t INTERRUPT_ENABLE_2{0x03};
// FIFO
constexpr uint8_t FIFO_WRITE_POINTER{0x04};
constexpr uint8_t FIFO_OVERFLOW_COUNTER{0x05};
constexpr uint8_t FIFO_READ_POINTER{0x06};
constexpr uint8_t FIFO_DATA_REGISTER{0x07};
// CONFIGURATION
constexpr uint8_t FIFO_CONFIGURATION{0x08};
constexpr uint8_t MODE_CONFIGURATION{0x09};
constexpr uint8_t SPO2_CONFIGURATION{0x0A};
constexpr uint8_t LED_CONFIGURATION_1{0x0C};
constexpr uint8_t LED_CONFIGURATION_2{0x0D};
// MultiLED
constexpr uint8_t MULTI_LED_MODE_CONTROL_12{0x11};
constexpr uint8_t MULTI_LED_MODE_CONTROL_34{0x12};
// TEMPERATURE
constexpr uint8_t TEMP_INTEGER{0x1F};
constexpr uint8_t TEMP_FRACTION{0x20};
constexpr uint8_t TEMP_CONFIGURATION{0x21};
// PART ID
constexpr uint8_t READ_REVISION_ID{0xFE};
constexpr uint8_t READ_PART_ID{0xFF};

}  // namespace command
///@endcond

}  // namespace max30102

/*!
  @class m5::unit::UnitMAX30102
  @brief Pulse oximetry and heart-rate sensor
  @note The only single measurement is temperature; other data is constantly measured and stored
*/
class UnitMAX30102 : public Component, public PeriodicMeasurementAdapter<UnitMAX30102, max30102::Data> {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitMAX30102, 0x57);

public:
    /*!
      @struct config_t
      @brief Settings for begin
      @warning Note that some combinations of sampling_rate and pulse_width are invalid. See alse SpO2 configuration
     */
    struct config_t {
        /*!
          @brief Operating mode if start on begin
          @warning In MultiLED mode, only the mode setting is performed
          @warning The other settings are ignored, so you need to set and start them yourself
         */
        max30102::Mode mode{max30102::Mode::SPO2};
        //! Start periodic measurement on begin?
        bool start_periodic{true};
        //! ADC range if start on begin
        max30102::SpO2ADCRange adc_range{max30102::SpO2ADCRange::nA4096};
        //! Sampling rate if start on begin
        max30102::Sampling sampling_rate{max30102::Sampling::Rate400};
        //! LED pulse width if start on begin
        max30102::LEDPulseWidth pulse_width{max30102::LEDPulseWidth::PW411};
        //!  LED current for IR if start on begin
        uint8_t ir_current{0x1F};
        //! LED current for Red if start on begin (only SpO2 MODE)
        uint8_t red_current{0x1F};
        //! FIFO sampling average if start on begin
        max30102::FIFOSampling fifo_sampling_average{max30102::FIFOSampling::Average4};
    };

    explicit UnitMAX30102(const uint8_t addr = DEFAULT_ADDRESS)
        : Component(addr), _data{new m5::container::CircularBuffer<max30102::Data>(max30102::MAX_FIFO_DEPTH)}
    {
        auto ccfg        = component_config();
        ccfg.clock       = 400 * 1000U;
        ccfg.stored_size = max30102::MAX_FIFO_DEPTH;
        component_config(ccfg);
    }
    virtual ~UnitMAX30102()
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
    inline uint32_t ir() const
    {
        return !empty() ? oldest().ir() : 0;
    }
    //! @brief Oldest Red
    inline uint32_t red() const
    {
        return !empty() ? oldest().red() : 0;
    }
    /*!
      @brief Number of data last retrieved
      @note The number of data retrieved by the latest update, not all data accumulated
      @sa available()
    */
    inline uint8_t retrived() const
    {
        return _retrived;
    }
    /*!
      @brief The number of samples lost
      @note It saturates at (MAX_FIFO_DEPTH -1)
     */
    inline uint8_t overflow() const
    {
        return _overflow;
    }
    ///@}

    /*!
      @brief Calculate the sampling rate from the current settings
      @retval
      @retval >= 0 Sampling rate
      @note Calculate by FIFO average and SpO2 sampling rate
     */
    uint32_t caluculateSamplingRate();

    ///@name Periodic measurement
    ///@{
    /*!
      @brief Start periodic measurement in the current settings
      @return True if successful
    */
    inline bool startPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitMAX30102, max30102::Data>::startPeriodicMeasurement();
    }
    /*!
      @brief Start periodic measurement
      @param mode Operation mode
      @param range ADC Range
      @param rate  Sampling rate
      @param width LED pulse width
      @param avg FIFO sampling average
      @param ir_current IR Led control (for All mode)
      @param red_current RED Led control (for Mode::SPO2 and Mode::MultiLED)
      @return True if successful
      @warning Note that some combinations of rate and width are invalid. See also datasheet
    */
    inline bool startPeriodicMeasurement(const max30102::Mode mode, const max30102::SpO2ADCRange range,
                                         const max30102::Sampling rate, const max30102::LEDPulseWidth width,
                                         const max30102::FIFOSampling avg, const uint8_t ir_current,
                                         const uint8_t red_current)
    {
        return PeriodicMeasurementAdapter<UnitMAX30102, max30102::Data>::startPeriodicMeasurement(
            mode, range, rate, width, avg, ir_current, red_current);
    }
    /*!
      @brief Stop periodic measurement
      @return True if successful
      @note Entering Power-save mode
    */
    inline bool stopPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitMAX30102, max30102::Data>::stopPeriodicMeasurement();
    }
    ///@}

    ///@name Mode configuration
    ///@{
    /*!
      @brief Read the operation mode
      @param[out] mode Mode
      @return True if successful
     */
    bool readMode(max30102::Mode& mode);
    /*!
      @brief Write the operation mode
      @param mode Mode
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeMode(const max30102::Mode mode);
    /*!
      @brief Read the shutdown control
      @param[out] shdn Shutdown control state (true:Power-save mode)
      @return True if successful
     */
    bool readShutdownControl(bool& shdn);
    /*!
      @brief Write the shutdown control
      @param shdn Shutdown control state (true:Power-save mode)
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeShutdownControl(const bool shdn);
    ///@}

    /*!
      @note Note that there are different combinations that can be set depending on the mode
      - Mode:SPO2
      |Sampling\LEDPulseWidth|69 |118 |215 |411|
      |---|---|---|---|---|
      |  50|o|o|o|o|
      | 100|o|o|o|o|
      | 200|o|o|o|o|
      | 400|o|o|o|o|
      | 800|o|o|o|x|
      |1000|o|o|x|x|
      |1600|o|x|x|x|
      |3200|x|x|x|x|
      - Mode:HROnly
      |Sampling\LEDPulseWidth|69 |118 |215 |411|
      |---|---|---|---|---|
      |  50|o|o|o|o|
      | 100|o|o|o|o|
      | 200|o|o|o|o|
      | 400|o|o|o|o|
      | 800|o|o|o|o|
      |1000|o|o|o|o|
      |1600|o|o|o|x|
      |3200|o|x|x|x|
    */
    ///@name SpO2 configuration
    ///@{
    /*!
      @brief Read the SpO2 configuration
      @param[out] range ADC rRange
      @param[out] rate Sampling rate
      @param[out] width LED pulse width
      @return True if successful
     */
    bool readSpO2Configuration(max30102::SpO2ADCRange& range, max30102::Sampling& rate, max30102::LEDPulseWidth& width);
    //! @brief Read the ADC range
    inline bool readSpO2ADCRange(max30102::SpO2ADCRange& range)
    {
        max30102::Sampling rate{};
        max30102::LEDPulseWidth width{};
        return readSpO2Configuration(range, rate, width);
    }
    //! @brief Read the sampling rate
    inline bool readSpO2SamplingRate(max30102::Sampling& rate)
    {
        max30102::SpO2ADCRange range{};
        max30102::LEDPulseWidth width{};
        return readSpO2Configuration(range, rate, width);
    }
    //! @brief Read the LED pulse width
    inline bool readSpO2LEDPulseWidth(max30102::LEDPulseWidth& width)
    {
        max30102::SpO2ADCRange range{};
        max30102::Sampling rate{};
        return readSpO2Configuration(range, rate, width);
    }
    /*!
      @brief Write the SpO2 configuration
      @param range ADC rRange
      @param rate Sampling rate
      @param width LED pulse width
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeSpO2Configuration(const max30102::SpO2ADCRange range, const max30102::Sampling rate,
                                const max30102::LEDPulseWidth width);
    //! @brief Write the ADC range
    bool writeSpO2ADCRange(const max30102::SpO2ADCRange range);
    //! @brief Write the sampling rate
    bool writeSpO2SamplingRate(const max30102::Sampling rate);
    //! @brief Write the LED pulse width
    bool writeSpO2LEDPulseWidth(const max30102::LEDPulseWidth width);
    ///@}

    ///@note Note that the target LEDs differ depending on the mode
    ///@note In SPO2/HRonly mode, slot1(LED1):Red slot2(LED2):IR
    ///@note In MultiMode, it is the same as the Slot setting
    ///@name LED Pulse Amplitude
    ///@{
    /*!
      @brief Read the LED current
      @param[out] raw Raw value
      @param slot Slot index (0:LED1 1:LED2)
      @return True if successful
     */
    inline bool readLEDCurrent(uint8_t& raw, const uint8_t slot)
    {
        return read_led_current(slot, raw);
    }
    /*!
      @brief Read the LED current
      @param[out] mA Current (mA)
      @param slot Slot index (0:LED1 1:LED2)
      @return True if successful
      @note mA Value in increments of 0.2
     */
    inline bool readLEDCurrent(float& mA, const uint8_t slot)
    {
        return read_led_current(slot, mA);
    }
    /*!
      @brief Write the LED current
      @param slot Slot index (0:LED1 1:LED2)
      @param raw Raw value
      @return True if successful
     */
    inline bool writeLEDCurrent(const uint8_t slot, const uint8_t raw)
    {
        return write_led_current(slot, raw);
    }
    /*!
      @brief Write the LED current
      @param slot Slot index (0:LED1 1:LED2)
      @param mA Current (mA) 0 - 51.0f
      @return True if successful
      @note mA Value in increments of 0.2
     */
    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, std::nullptr_t>::type = nullptr>
    inline bool writeLEDCurrent(const uint8_t slot, const T mA)
    {
        return write_led_current(slot, (float)mA);
    }
    ///@}

    ///@warning Available only in MultiLED Mode
    ///@warning For MAX30102 only slots 1 and 2 are valid
    ///@name Multi-LED Mode Control
    ///@{
    /*!
      @brief Read the the MultiLED Mode form Slot 1-2
      @param[out] slot1 Slot1 mode
      @param[out] slot2 Slot2 mode
      @return True if successful
     */
    bool readMultiLEDModeControl(max30102::Slot& slot1, max30102::Slot& slot2);
    /*!
      @brief Write the MultiLED Mode to Slot 1-2
      @param slot1 Slot1 mode
      @param slot2 Slot2 mode
      @return True if successful
      @warning The slots should be enabled in order (i.e., SLOT1 should not be disabled if SLOT2 is enabled)
      @warning During periodic detection runs, an error is returned
     */
    bool writeMultiLEDModeControl(const max30102::Slot slot1, const max30102::Slot slot2);
    ///@}

    ///@name Measurement temperature
    ///@{
    /*!
      @brief Measure tempeature single shot
      @param[out] temp Temperature(Celsius)
      @return True if successful
      @warning Blocking until measured about 29 ms
      @warning Does not work in power-save mode
      @sa m5::unit::MAX30102readShutdownControl
     */
    bool measureTemperatureSingleshot(max30102::TemperatureData& td);
    ///@}

    ///@name FIFO
    ///@{
    bool readFIFOConfiguration(max30102::FIFOSampling& avg, bool& rollover, uint8_t& almostFull);
    bool writeFIFOConfiguration(const max30102::FIFOSampling avg, const bool rollover, const uint8_t almostFull);

    //! @brief Read the FIFO read pointer
    inline bool readFIFOReadPointer(uint8_t& rptr)
    {
        rptr = 0xFF;
        return read_register8(max30102::command::FIFO_READ_POINTER, rptr);
    }
    //! @brief Write the FIFO read pointer
    inline bool writeFIFOReadPointer(const uint8_t rptr)
    {
        return writeRegister8(max30102::command::FIFO_READ_POINTER, rptr);
    }
    //! @brief Read the FIFO write pointer
    inline bool readFIFOWritePointer(uint8_t& wptr)
    {
        wptr = 0xFF;
        return read_register8(max30102::command::FIFO_WRITE_POINTER, wptr);
    }
    //! @brief Write the FIFO write pointer
    inline bool writeFIFOWritePointer(const uint8_t wptr)
    {
        return writeRegister8(max30102::command::FIFO_WRITE_POINTER, wptr);
    }
    //! @brief Read the FIFO overflow counter
    inline bool readFIFOOverflowCounter(uint8_t& cnt)
    {
        cnt = 0xFF;
        return read_register8(max30102::command::FIFO_OVERFLOW_COUNTER, cnt);
    }
    //! @brief Write the FIFO overflow counter
    inline bool writeFIFOOverflowCounter(const uint8_t cnt)
    {
        return writeRegister8(max30102::command::FIFO_OVERFLOW_COUNTER, cnt);
    }
    //! @brief Reset FIFO pointer and counter
    inline bool resetFIFO()
    {
        return reset_FIFO();
    }
    ///@}

    /*!
      @brief Reset
      @return True if successful
      @warning Blocked until the reset process is completed
     */
    bool reset();

    /*!
      @brief Read the revision ID
      @param[out] rev Revision
      @return True if successful
      @note 2-digit hexadecimal number (00 to FF) for part revision identification
      @note Contact Maxim Integrated for the revision ID number assigned for your product
     */
    bool readRevisionID(uint8_t& rev);

protected:
    bool read_register(const uint8_t reg, uint8_t* buf, const size_t len);
    bool read_register8(const uint8_t reg, uint8_t& v);

    bool start_periodic_measurement();
    bool start_periodic_measurement(const max30102::Mode mode, const max30102::SpO2ADCRange range,
                                    const max30102::Sampling rate, const max30102::LEDPulseWidth width,
                                    const max30102::FIFOSampling avg, const uint8_t ir_current,
                                    const uint8_t red_current);
    bool stop_periodic_measurement();

    bool write_mode_configuration(const max30102::ModeConfiguration& sc);
    bool write_spo2_configuration(const max30102::SpO2Configuration& sc);

    bool read_led_current(const uint8_t idx, uint8_t& raw);
    bool read_led_current(const uint8_t idx, float& mA);
    bool write_led_current(const uint8_t idx, const uint8_t raw);
    bool write_led_current(const uint8_t idx, const float mA);

    bool write_fifo_sampling_average(const max30102::FIFOSampling avg);

    bool read_FIFO();
    bool reset_FIFO(const bool circling_read_ptr = true);

    bool read_measurement_temperature(max30102::TemperatureData& td);

    M5_UNIT_COMPONENT_PERIODIC_MEASUREMENT_ADAPTER_HPP_BUILDER(UnitMAX30102, max30102::Data);

protected:
    std::unique_ptr<m5::container::CircularBuffer<max30102::Data>> _data{};
    max30102::Mode _mode{};
    uint8_t _retrived{}, _overflow{};
    uint32_t _mask{};  // Valid bits based on ADC range
    max30102::Slot _slot[2]{};
    config_t _cfg{};
};

}  // namespace unit
}  // namespace m5
#endif
