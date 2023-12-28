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
#include <iostream>
#include "Si5351FSKModulator.h"

#include "hardware/i2c.h"
#include "hello-pico-si5351/si5351.h"

using namespace std;
using namespace radlib;

Si5351FSKModulator::Si5351FSKModulator(ClockInterface& clock,
    uint32_t markOffset, uint32_t spaceOffset)
:   _clock(clock),
    _markOffset(markOffset),
    _spaceOffset(spaceOffset) {
}

void Si5351FSKModulator::setBaseFreq(uint32_t freqHz) {
    _baseFreq = freqHz;
}

void Si5351FSKModulator::enable(bool on) {
    si_enable(0, on);
}

void Si5351FSKModulator::sendCW() {
    si_evaluate(0, _baseFreq + _markOffset + _correction);
    _clock.sleepMs(5000);
}

void Si5351FSKModulator::sendSilence(uint32_t us) {
    si_enable(0, false);
    _clock.sleepMs(us / 1000);
    si_enable(0, true);
}

void Si5351FSKModulator::sendMark(uint32_t us) {
    si_evaluate(0, _baseFreq + _markOffset + _correction);
    _clock.sleepMs(us / 1000);
}

void Si5351FSKModulator::sendSpace(uint32_t us) {
    si_evaluate(0, _baseFreq + _spaceOffset + _correction);
    _clock.sleepMs(us / 1000);
}

