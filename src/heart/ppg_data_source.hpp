/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file ppg_data_source.hpp
  @brief Interface for PPG data providers
*/
#ifndef M5_UNIT_HEART_HEART_PPG_DATA_SOURCE_HPP
#define M5_UNIT_HEART_HEART_PPG_DATA_SOURCE_HPP

#include "ppg_types.hpp"

namespace m5 {
namespace heart {

/*!
  @class PPGDataSource
  @brief Thin contract between PPG units and the processing layer
  @details Implementations report what they provide and hand out unprocessed samples in oldest-first order
  (pop semantics). User-defined sources only need to implement ppgSpec() and readPPG().
 */
class PPGDataSource {
public:
    virtual ~PPGDataSource() = default;

    /*!
      @brief What this source provides
      @return Channels, sampling rate and unit (sampling rate is 0 if not measuring)
     */
    virtual PPGSpec ppgSpec() const = 0;
    /*!
      @brief Pop the oldest unprocessed sample
      @param[out] out Sample destination
      @return True if a sample was stored in out
     */
    virtual bool readPPG(PPGSample& out) = 0;
    /*!
      @brief Did the latest read of the device detect a FIFO overflow?
      @return True if samples were lost
     */
    virtual bool ppgOverflowed() const
    {
        return false;
    }
};

}  // namespace heart
}  // namespace m5
#endif
