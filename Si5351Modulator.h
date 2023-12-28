/*
Copyright (C) 2023 - Bruce MacKinnon KC1FSZ

This program is free software: you can redistribute it and/or modify it under 
the terms of the GNU General Public License as published by the Free 
Software Foundation, either version 3 of the License, or (at your option) any 
later version.

This program is distributed in the hope that it will be useful, but WITHOUT 
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS 
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef _Si5351Modulator_h
#define _Si5351Modulator_h

#include <cstdint>
#include <iostream>

#include "radlib/util/ClockInterface.h"
#include "hello-scamp/Modulator.h"

namespace scamp {

/**
 * IMPORTANT NOTES ON TX BEHAVIOR:
 * 
 * 1. When a sendMark() or sendSpace() is complete the transmitor is
 *    left in the transmitting state.
 * 2. Transition from mark->space or space->mark is done as smoothly
 *    as possible (i.e. phase coherent)
*/
class Si5351Modulator : public Modulator {
public:

    Si5351Modulator(radlib::ClockInterface& clock, uint32_t markOffset, uint32_t spaceOffset);
    virtual ~Si5351Modulator();

    void setBaseFreq(uint32_t freqHz);
    uint32_t getBaseFreq() const { return _baseFreq; };
    void enable(bool on);

    void sendCW();
    void sendCQ();

    void sendSilence(uint32_t us);
    void sendMark(uint32_t us);
    void sendSpace(uint32_t us);

private:

    radlib::ClockInterface& _clock;
    uint32_t _baseFreq = 0;
    uint32_t _markOffset;
    uint32_t _spaceOffset;
    uint32_t _correction = 600;
};

}

#endif

