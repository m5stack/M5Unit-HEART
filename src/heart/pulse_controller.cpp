/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file pulse_controller.cpp
  @brief Pulls samples from a PPG data source and processes them
*/
#include "pulse_controller.hpp"

namespace m5 {
namespace heart {

PulseController::PulseController() : PulseController(PulseProcessor::config_t{})
{
}

PulseController::PulseController(const PulseProcessor::config_t& cfg) : _cfg(cfg), _processor(0.0f, 0, cfg)
{
}

bool PulseController::add(PPGDataSource& src)
{
    const PPGSpec spec = src.ppgSpec();
    if (!(spec.sampling_rate_hz > 0.0f)) {
        return false;
    }
    _src = &src;
    rebuild(spec);
    return true;
}

uint32_t PulseController::update()
{
    _beat = false;
    if (!_src) {
        return 0;
    }
    const PPGSpec spec = _src->ppgSpec();
    const float rate   = (spec.sampling_rate_hz > 0.0f) ? spec.sampling_rate_hz : 0.0f;
    if (rate != _processor.samplingRate() || (spec.channels & 0x07) != _processor.channels()) {
        rebuild(spec);
    }

    uint32_t count{};
    PPGSample s{};
    while (_src->readPPG(s)) {
        // Reset only when new samples arrive, so that repeated calls between device reads do not reset again
        if (!count && _src->ppgOverflowed()) {
            _processor.reset();
        }
        _processor.push(s);
        _beat |= _processor.isBeat();
        ++count;
        s = PPGSample{};
    }
    return count;
}

void PulseController::rebuild(const PPGSpec& spec)
{
    _processor = PulseProcessor(spec.sampling_rate_hz, spec.channels, _cfg);
    _beat      = false;
}

}  // namespace heart
}  // namespace m5
