#include <iostream>
#include "Si5351Modulator.h"

#include "hardware/i2c.h"
#include "hello-pico-si5351/si5351.h"

using namespace std;

namespace scamp {

Si5351Modulator::Si5351Modulator(ClockInterface& clock,
    uint32_t markOffset, uint32_t spaceOffset, uint32_t holdMs)
:   _clock(clock),
    _markOffset(markOffset),
    _spaceOffset(spaceOffset),
    _holdMs(holdMs) {
}

Si5351Modulator::~Si5351Modulator() {    
}

void Si5351Modulator::setBaseFreq(uint32_t freqHz) {
    _baseFreq = freqHz;
}

void Si5351Modulator::enable(bool on) {
    si_enable(0, on);
}

void Si5351Modulator::sendCW() {
    si_evaluate(0, _baseFreq + _markOffset + _correction);
    _clock.sleepMs(1000);
}

void Si5351Modulator::sendCQ() {
}

void Si5351Modulator::sendSilence() {
    _clock.sleepMs(_holdMs);
}

void Si5351Modulator::sendMark() {
    si_evaluate(0, _baseFreq + _markOffset + _correction);
    _clock.sleepMs(_holdMs);
}

void Si5351Modulator::sendSpace() {
    si_evaluate(0, _baseFreq + _spaceOffset + _correction);
    _clock.sleepMs(_holdMs);
}

}


