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
    SpO2,           //!< SPO2 and HR enabled
};

/*!
  @enum Sampling
  @brief Sampling rate for pulse/conversion
  @details Unit is the number of sample per second
 */
enum class Sampling : uint8_t {
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
  @enum LEDPulse
  @brief  LED pulse width (the IR and RED have the same pulse width)
*/
enum class LEDPulse : uint8_t {
    Width200,   //!< 200 us (ADC 13 bits)
    Width400,   //!< 400 us (ADC 14 bits)
    Width800,   //!< 800 us (ADC 15 bits)
    Width1600,  //!< 1600 us (ADC 16 bits)
};

/*!
  @enum LED
  @brief LED current control
 */
enum class LED : uint8_t {
    Current0_0,   //!< 0,0 mA
    Current4_4,   //!< 4,4 mA
    Current7_6,   //!< 7.6 mA
    Current11_0,  //!< 11.0 mA
    Current14_2,  //!< 14.2 mA
    Current17_4,  //!< 17.4 mA
    Current20_8,  //!< 20,8 mA
    Current24_0,  //!< 24.0 mA
    Current27_1,  //!< 27.1 mA
    Current30_6,  //!< 30.6 mA
    Current33_8,  //!< 33.8 mA
    Current37_0,  //!< 37.0 mA
    Current40_2,  //!< 40.2 mA
    Current43_6,  //!< 43.6 mA
    Current46_8,  //!< 46.8 mA
    Current50_0,  //!< 50.0 mA
};

constexpr uint8_t MAX_FIFO_DEPTH{16};  //!< @brief FIFO depth

/*!
  @struct Data
  @brief Measurement data group
 */
struct Data {
    std::array<uint8_t, 4> raw{};  // [0...1]:IR [2...3]:Red
    inline uint16_t ir() const
    {
        return m5::types::big_uint16_t(raw[0], raw[1]).get();
    }
    inline uint16_t red() const
    {
        return m5::types::big_uint16_t(raw[2], raw[3]).get();
    }
};

/*!
  @struct TemperatureData
  @brief Measurement data group for temperature
 */
struct TemperatureData {
    std::array<uint8_t, 2> raw{0xFF, 0xFF};  // [0]:integer [1]:fraction
    //! @brief Temperature (Celsius)
    inline float temperature() const
    {
        return celsius();
    }
    //! @brief Temperature (Celsius)
    inline float celsius() const
    {
        return (raw[0] != 0xFF) ? (int8_t)raw[0] + raw[1] * 0.0625f : std::numeric_limits<float>::quiet_NaN();
    }
    //! @brief temperature (Fahrenheit)
    inline float fahrenheit() const
    {
        return celsius() * 9.0f / 5.0f + 32.f;
    }
};

///@cond
namespace command {
// STATUS
constexpr uint8_t READ_INTERRUPT_STATUS{0x00};
constexpr uint8_t INTERRUPT_ENABLE{0x01};
// FIFO
constexpr uint8_t FIFO_WRITE_POINTER{0x02};
constexpr uint8_t FIFO_OVERFLOW_COUNTER{0x03};
constexpr uint8_t FIFO_READ_POINTER{0x04};
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
constexpr uint8_t READ_PART_ID{0xFF};

}  // namespace command

struct SpO2Configuration;
///@endcond

}  // namespace max30100

/*!
  @class m5::unit::UnitMAX30100
  @brief Pulse oximetry and heart-rate sensor
  @note The only single measurement is temperature; other data is constantly measured and stored
*/
class UnitMAX30100 : public Component, public PeriodicMeasurementAdapter<UnitMAX30100, max30100::Data> {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitMAX30100, 0x57);

public:
    /*!
      @struct config_t
      @brief Settings for begin
      @warning Note that some combinations of sampling_rate and pulse_width are invalid. See also datasheet
     */
    struct config_t {
        //! Start periodic measurement on begin?
        bool start_periodic{true};
        //! Operating mode if start on begin
        max30100::Mode mode{max30100::Mode::SpO2};
        //! Sampling rate if start on begin
        max30100::Sampling rate{max30100::Sampling::Rate100};
        //! Led pulse width if start on begin
        max30100::LEDPulse width{max30100::LEDPulse::Width1600};
        //!  Led current for IR if start on begin
        m5::unit::max30100::LED ir_current{max30100::LED::Current27_1};
        //! The SpO2 ADC resolution if start on begin
        bool high_resolution{true};
        //! Led current for Red if start on begin (only SpO2)
        m5::unit::max30100::LED red_current{max30100::LED::Current27_1};
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
      @note It saturates at (MAX_FIFO_DEPTH - 1)
     */
    inline uint8_t overflow() const
    {
        return _overflow;
    }
    ///@}

    /*!
      @brief Calculate the sampling rate from the current settings
      @return >= 0 Sampling rate
      @note Calculate by SpO2 sampling rate
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
        return PeriodicMeasurementAdapter<UnitMAX30100, max30100::Data>::startPeriodicMeasurement();
    }
    /*!
      @brief Start periodic measurement
      @param mode Operation mode
      @param rate  Sampling rate
      @param width Pulse width
      @param ir_current IR Led control
      @param high_resolution (only SpO2)
      @param red_current RED Led control (only SpO2)
      @return True if successful
      @warning Note that some combinations of rate and width are invalid. See also datasheet
    */
    inline bool startPeriodicMeasurement(const max30100::Mode mode, const max30100::Sampling rate,
                                         const max30100::LEDPulse width, const max30100::LED ir_current,
                                         const bool high_resolution      = false,
                                         const max30100::LED red_current = max30100::LED::Current0_0)
    {
        return PeriodicMeasurementAdapter<UnitMAX30100, max30100::Data>::startPeriodicMeasurement(
            mode, rate, width, ir_current, high_resolution, red_current);
    }
    /*!
      @brief Stop periodic measurement
      @return True if successful
    */
    inline bool stopPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitMAX30100, max30100::Data>::stopPeriodicMeasurement();
    }
    ///@}

    ///@name Mode Configuration
    ///@{
    /*!
      @brief Read the operation mode
      @param[out] mode Mode
      @return True if successful
    */
    bool readMode(max30100::Mode& mode);
    /*!
      @brief Write the operation mode
      @param mode Mode
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeMode(const max30100::Mode mode);
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
      |Sampling\LEDPulseWidth|200 |400 |800 |1600|
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
      |Sampling\LEDPulseWidth|200 |400 |800 |1600|
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
    ///@name SpO2 Configuration
    ///@{
    /*!
      @brief Read the SpO2 configuration
      @param[out] resolution SpO2 ADC resolution (true; high)
      @param[out] rate Sampling rate
      @param[out] width LED pulse width
      @return True if successful
     */
    bool readSpO2Configuration(bool& resolution, max30100::Sampling& rate, max30100::LEDPulse& width);
    /*!
      @brief Write the SpO2 configuration
      @param resolution SpO2 ADC resolution (true; high)
      @param rate Sampling rate
      @param width LED pulse width
      @return True if successful
     */
    bool writeSpO2Configuration(const bool resolution, const max30100::Sampling rate, const max30100::LEDPulse width);

    //! @brief Read the SpO2 resolution mode
    inline bool readSpO2HighResolution(bool& enabled)
    {
        max30100::Sampling rate{};
        max30100::LEDPulse width{};
        return readSpO2Configuration(enabled, rate, width);
    }
    //! @brief Write the SpO2 resolution mode
    bool writeSpO2HighResolution(const bool enabled);
    //! @brief Write the SpO2 high resolution mode to enable
    inline bool writeSpO2HighResolutionEnable()
    {
        return writeSpO2HighResolution(true);
    }
    //! @brief Write the SpO2 high resolution mode to disable
    inline bool writeSpO2HighResolutionDisable()
    {
        return writeSpO2HighResolution(false);
    }
    //! @brief Read the sampling rate
    inline bool readSpO2SamplingRate(max30100::Sampling& rate)
    {
        bool enabled{};
        max30100::LEDPulse width{};
        return readSpO2Configuration(enabled, rate, width);
    }
    //! @brief Write the sampling rate
    bool writeSpO2SamplingRate(const max30100::Sampling rate);
    //! @brief Write LED pulse width
    inline bool readSpO2LEDPulseWidth(max30100::LEDPulse& width)
    {
        bool enabled{};
        max30100::Sampling rate{};
        return readSpO2Configuration(enabled, rate, width);
    }
    //! @brief Write LED pulse width
    bool writeSpO2LEDPulseWidth(const max30100::LEDPulse width);
    ///@}

    ///@warning In the heart-rate only mode, the red LED is inactive
    // @warning and only the IR LED is used to capture optical data and determine the heart rate
    ///@name LED Configuration
    ///@{
    /*!
      @brief Read the LED curremt
      @param[out] ir_current IR current
      @param[out] red_current Red current
      @return True if successful
    */
    bool readLEDCurrent(max30100::LED& ir_current, max30100::LED& red_current);
    /*!
      @brief Write the LED current
      @param ir_current IR current
      @param red_current Red current
      @return True if successful
    */
    bool writeLEDCurrent(const max30100::LED ir_current, const max30100::LED red_current);
    ///@}

    ///@name Measurement temperature
    ///@{
    /*!
      @brief Measure tempeature single shot
      @param[out] td TemperatureData
      @return True if successful
      @warning Blocking until measured about 29 ms
      @warning Does not work in power-save mode
      @sa m5::unit::MAX30100::readShutdownControl
     */
    bool measureTemperatureSingleshot(max30100::TemperatureData& td);
    ///@}

    ///@name FIFO
    ///@{
    //! @brief Read the FIFO read pointer
    inline bool readFIFOReadPointer(uint8_t& rptr)
    {
        rptr = 0xFF;
        return read_register8(max30100::command::FIFO_READ_POINTER, rptr);
    }
    //! @brief Write the FIFO read pointer
    inline bool writeFIFOReadPointer(const uint8_t rptr)
    {
        return writeRegister8(max30100::command::FIFO_READ_POINTER, rptr);
    }
    //! @brief Read the FIFO write pointer
    inline bool readFIFOWritePointer(uint8_t& wptr)
    {
        wptr = 0xFF;
        return read_register8(max30100::command::FIFO_WRITE_POINTER, wptr);
    }
    //! @brief Write the FIFO write pointer
    inline bool writeFIFOWritePointer(const uint8_t wptr)
    {
        return writeRegister8(max30100::command::FIFO_WRITE_POINTER, wptr);
    }
    //! @brief Read the FIFO overflow counter
    inline bool readFIFOOverflowCounter(uint8_t& cnt)
    {
        cnt = 0xFF;
        return read_register8(max30100::command::FIFO_OVERFLOW_COUNTER, cnt);
    }
    //! @brief Write the FIFO overflow counter
    inline bool writeFIFOOverflowCounter(const uint8_t cnt)
    {
        return writeRegister8(max30100::command::FIFO_OVERFLOW_COUNTER, cnt);
    }
    //! @brief Reset FIFO pointer and counter
    bool resetFIFO();
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
    bool start_periodic_measurement(const max30100::Mode mode, const max30100::Sampling rate,
                                    const max30100::LEDPulse width, const max30100::LED ir_current,
                                    const bool resolution, const max30100::LED red_current);
    bool stop_periodic_measurement();

    bool read_FIFO();
    bool read_measurement_temperature(max30100::TemperatureData& td);

    bool write_spo2_configuration(const max30100::SpO2Configuration& sc);

    M5_UNIT_COMPONENT_PERIODIC_MEASUREMENT_ADAPTER_HPP_BUILDER(UnitMAX30100, max30100::Data);

protected:
    max30100::Mode _mode{max30100::Mode::None};
    uint8_t _retrived{}, _overflow{};
    std::unique_ptr<m5::container::CircularBuffer<max30100::Data>> _data{};

    config_t _cfg{};
};

}  // namespace unit
}  // namespace m5

#endif
