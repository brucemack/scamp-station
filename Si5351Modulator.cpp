#include <iostream>
#include "Si5351Modulator.h"

#include "hardware/i2c.h"
#include "hello-pico-si5351/si5351.h"

using namespace std;

namespace scamp {

Si5351Modulator::Si5351Modulator(ClockInterface& clock,
    uint32_t markOffset, uint32_t spaceOffset)
:   _clock(clock),
    _markOffset(markOffset),
    _spaceOffset(spaceOffset) {
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
    _clock.sleepMs(5000);
}

void Si5351Modulator::sendCQ() {
}

void Si5351Modulator::sendSilence(uint32_t us) {
    si_enable(0, false);
    _clock.sleepUs(us);
    si_enable(0, true);
}

void Si5351Modulator::sendMark(uint32_t us) {
    si_evaluate(0, _baseFreq + _markOffset + _correction);
    _clock.sleepUs(us);
}

void Si5351Modulator::sendSpace(uint32_t us) {
    si_evaluate(0, _baseFreq + _spaceOffset + _correction);
    _clock.sleepUs(us);
}

}

