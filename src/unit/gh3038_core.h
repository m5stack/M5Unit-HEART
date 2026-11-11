/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file gh3038_core.h
  @brief C API of the precompiled GH3038 core (libm5gh3038core.a)
  @note Values of mode, rate, color and gain level are the same as m5::unit::gh3038::Mode, SamplingRate, Color
  and Gain
*/
#ifndef M5_GH3038_CORE_H
#define M5_GH3038_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M5_GH3038_CORE_API_VERSION (1U)    //!< @brief Must match m5_gh3038_api_version()
#define M5_GH3038_CORE_STATE_SIZE  (128U)  //!< @brief Bytes of m5_gh3038_state_t

/*!
  @enum m5_gh3038_result
  @brief Result codes
 */
enum m5_gh3038_result {
    M5_GH3038_OK            = 0,   //!< Success
    M5_GH3038_ERR_IO        = -1,  //!< I2C or RST access failed
    M5_GH3038_ERR_NOT_READY = -2,  //!< The chip did not become ready after reset
    M5_GH3038_ERR_ARG       = -3,  //!< Invalid argument
    M5_GH3038_ERR_STATE     = -4,  //!< Not initialized, or not allowed in the current state
};

//! @brief Opaque state (allocated by the caller)
typedef struct {
    uint64_t opaque[M5_GH3038_CORE_STATE_SIZE / 8U];
} m5_gh3038_state_t;

/*!
  @struct m5_gh3038_io_t
  @brief Callbacks provided by the caller
 */
typedef struct {
    void* user;  //!< Passed to every callback
    //! Write len bytes to the 16-bit register reg (STOP at the end)
    bool (*write)(void* user, uint16_t reg, const uint8_t* data, uint32_t len);
    //! Read len bytes from the 16-bit register reg (repeated start after the register address)
    bool (*read)(void* user, uint16_t reg, uint8_t* rx, uint32_t len);
    //! Wait for us microseconds
    void (*delay_us)(void* user, uint32_t us);
    //! Drive the RST pin (NULL: RST is not driven and soft reset is used)
    bool (*set_rst)(void* user, int level);
    //! Maximum bytes of one read (4 or more)
    uint32_t max_read;
} m5_gh3038_io_t;

/*!
  @struct m5_gh3038_sample_t
  @brief One sample of each driven LED
 */
typedef struct {
    int32_t raw[3];   //!< Signed ADC value [Green, Red, IR]
    uint8_t gain[3];  //!< Gain level at the measurement [Green, Red, IR]
    uint8_t valid;    //!< Valid bits (1 << color)
} m5_gh3038_sample_t;

//! @brief Called for each assembled sample in m5_gh3038_read()
typedef void (*m5_gh3038_sample_cb_t)(void* user, const m5_gh3038_sample_t* sample);

/*!
  @brief API version of the library
  @return Must be equal to M5_GH3038_CORE_API_VERSION
 */
uint32_t m5_gh3038_api_version(void);
/*!
  @brief sizeof(m5_gh3038_state_t) the library was built with
  @return Must be equal to sizeof(m5_gh3038_state_t)
 */
uint32_t m5_gh3038_state_size(void);
/*!
  @brief Maximum samples in the chip FIFO (for the buffer size)
  @return Maximum number of samples
 */
uint16_t m5_gh3038_max_samples(void);
/*!
  @brief Photodiode current (pA) of the raw value with the gain level
  @param raw Raw value of m5_gh3038_sample_t
  @param gain_level Gain level at the measurement (0...6, larger values are clamped to 6)
  @return Photodiode current (pA)
 */
float m5_gh3038_raw_to_pA(int32_t raw, uint8_t gain_level);
/*!
  @brief Full scale of the raw value
  @return Magnitude of the largest raw value
 */
int32_t m5_gh3038_raw_full_scale(void);

/*!
  @brief Initialize the state (no bus access)
  @param[out] s State to initialize
  @param io Callbacks (copied into the state)
  @return M5_GH3038_OK or a negative m5_gh3038_result
 */
int m5_gh3038_init(m5_gh3038_state_t* s, const m5_gh3038_io_t* io);
/*!
  @brief Replace io.user of an initialized state (e.g. after the owner object is moved)
  @param s State
  @param user New value of io.user
  @return M5_GH3038_OK or a negative m5_gh3038_result
  @note No bus access
 */
int m5_gh3038_set_io_user(m5_gh3038_state_t* s, void* user);
/*!
  @brief Reset the chip and write the common settings
  @param s State
  @param hard Hard reset with RST if true and set_rst exists, otherwise soft reset
  @return M5_GH3038_OK or a negative m5_gh3038_result
  @note If set_rst fails, RST may be left low
 */
int m5_gh3038_reset(m5_gh3038_state_t* s, bool hard);
/*!
  @brief Start the measurement
  @param s State
  @param mode Mode (0...2)
  @param rate Sampling rate (0...3)
  @return M5_GH3038_OK or a negative m5_gh3038_result
 */
int m5_gh3038_start(m5_gh3038_state_t* s, uint8_t mode, uint8_t rate);
/*!
  @brief Stop the measurement
  @param s State
  @return M5_GH3038_OK or a negative m5_gh3038_result
 */
int m5_gh3038_stop(m5_gh3038_state_t* s);
/*!
  @brief Is the measurement running?
  @param s State
  @return True if running (false if not initialized)
 */
bool m5_gh3038_running(const m5_gh3038_state_t* s);

/*!
  @brief Set the gain level of the color. Applied to the chip if running
  @param s State
  @param color Color (0...2)
  @param level Gain level (0...6)
  @return M5_GH3038_OK or a negative m5_gh3038_result
 */
int m5_gh3038_set_gain(m5_gh3038_state_t* s, uint8_t color, uint8_t level);
/*!
  @brief Get the gain level of the color (read from the chip if running)
  @param s State
  @param color Color (0...2)
  @param[out] level Gain level
  @return M5_GH3038_OK or a negative m5_gh3038_result
 */
int m5_gh3038_get_gain(m5_gh3038_state_t* s, uint8_t color, uint8_t* level);
/*!
  @brief Set the LED current code of the color. Applied to the chip if running
  @param s State
  @param color Color (0...2)
  @param code LED current code (0...255)
  @return M5_GH3038_OK or a negative m5_gh3038_result
 */
int m5_gh3038_set_led_current(m5_gh3038_state_t* s, uint8_t color, uint8_t code);
/*!
  @brief Get the LED current code of the color (read from the chip if running)
  @param s State
  @param color Color (0...2)
  @param[out] code LED current code
  @return M5_GH3038_OK or a negative m5_gh3038_result
 */
int m5_gh3038_get_led_current(m5_gh3038_state_t* s, uint8_t color, uint8_t* code);

/*!
  @brief Read the chip FIFO and call cb for each assembled sample
  @param s State
  @param cb Called for each sample
  @param user Passed to cb
  @param[out] count Number of samples passed to cb
  @param[out] overflowed True if the chip FIFO overflowed before this read
  @return M5_GH3038_OK or a negative m5_gh3038_result
  @warning Do not call any m5_gh3038_* function from cb (the chip is awake between the FIFO reads)
  @note cb, count and overflowed must not be NULL
  @note Do not call from an ISR
 */
int m5_gh3038_read(m5_gh3038_state_t* s, m5_gh3038_sample_cb_t cb, void* user, uint16_t* count, bool* overflowed);
/*!
  @brief Number of words in the chip FIFO
  @param s State
  @param[out] words Number of words
  @return M5_GH3038_OK or a negative m5_gh3038_result
 */
int m5_gh3038_fifo_level(m5_gh3038_state_t* s, uint16_t* words);

#ifdef __cplusplus
}
#endif
#endif
