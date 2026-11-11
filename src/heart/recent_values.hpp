/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file recent_values.hpp
  @brief Fixed-size history of the most recent values
*/
#ifndef M5_UNIT_HEART_HEART_RECENT_VALUES_HPP
#define M5_UNIT_HEART_HEART_RECENT_VALUES_HPP

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <limits>

namespace m5 {
namespace heart {

/*!
  @class RecentValues
  @brief Keeps the latest N values (older values are overwritten)
  @tparam N Capacity
 */
template <size_t N>
class RecentValues {
public:
    static_assert(N > 0, "N must be greater than 0");

    //! @brief Remove all values
    inline void clear()
    {
        _count = 0;
        _head  = 0;
    }
    //! @brief Add a value (the oldest is dropped if full)
    inline void push(const float v)
    {
        _values[_head] = v;
        _head          = (_head + 1) % N;
        if (_count < N) {
            ++_count;
        }
    }
    //! @brief Number of values
    inline size_t size() const
    {
        return _count;
    }
    //! @brief Median (NaN if empty)
    float median() const
    {
        if (!_count) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        float tmp[N]{};
        std::copy(_values, _values + _count, tmp);
        // Insertion sort (N is small; std::sort triggers a false -Warray-bounds on GCC 14 with ESP-IDF)
        for (size_t i = 1; i < _count; ++i) {
            const float v = tmp[i];
            size_t j      = i;
            for (; j > 0 && tmp[j - 1] > v; --j) {
                tmp[j] = tmp[j - 1];
            }
            tmp[j] = v;
        }
        return (_count & 1U) ? tmp[_count / 2] : (tmp[_count / 2 - 1] + tmp[_count / 2]) * 0.5f;
    }
    //! @brief Maximum minus minimum (NaN if empty)
    float spread() const
    {
        if (!_count) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        const auto mm = std::minmax_element(_values, _values + _count);
        return *mm.second - *mm.first;
    }

private:
    float _values[N]{};
    size_t _count{}, _head{};
};

}  // namespace heart
}  // namespace m5
#endif
