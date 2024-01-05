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
#ifndef _Si5351FSKModulator_h
#define _Si5351FSKModulator_h

#include <cstdint>
#include <iostream>

#include "radlib/util/ClockInterface.h"
#include "radlib/util/FSKModulator.h"

/**
 * IMPORTANT NOTES ON TX BEHAVIOR:
 * 
 * 1. When a sendMark() or sendSpace() is complete the transmitor is
 *    left in the transmitting state.
 * 2. Transition from mark->space or space->mark is done as smoothly
 *    as possible (i.e. phase coherent)
*/
class Si5351FSKModulator : public radlib::FSKModulator {
public:

    Si5351FSKModulator(radlib::ClockInterface& clock, 
        int32_t markOffset, int32_t spaceOffset);
    virtual ~Si5351FSKModulator() { }

    void setBaseFreq(int32_t freqHz);
    int32_t getBaseFreq() const { return _baseFreq; };

    void setOffsets(int32_t markOffset, int32_t spaceOffset) {
        _markOffset = markOffset;
        _spaceOffset = spaceOffset;
    }

    void enable(bool on);

    void sendCW();

    void setCorrection(int32_t c) {
        _correction = c;
    }

    void sendSilence(uint32_t us);
    void sendMark(uint32_t us);
    void sendSpace(uint32_t us);

private:

    radlib::ClockInterface& _clock;
    int32_t _baseFreq = 0;
    int32_t _markOffset;
    int32_t _spaceOffset;
    int32_t _correction = 600;
};

#endif

