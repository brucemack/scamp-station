#ifndef _Si5351Modulator_h
#define _Si5351Modulator_h

#include <cstdint>
#include <iostream>

#include "hello-scamp/Modulator.h"
#include "hello-lcd/ClockInterface.h"

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

    Si5351Modulator(ClockInterface& clock, uint32_t markOffset, uint32_t spaceOffset);
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

    ClockInterface& _clock;
    uint32_t _baseFreq = 0;
    uint32_t _markOffset;
    uint32_t _spaceOffset;
    uint32_t _correction = 600;
};

}

#endif

