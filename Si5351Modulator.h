#ifndef _Si5351Modulator_h
#define _Si5351Modulator_h

#include <cstdint>
#include <iostream>

#include "hello-scamp/Modulator.h"
#include "hello-lcd/ClockInterface.h"

namespace scamp {

class Si5351Modulator : public Modulator {
public:

    Si5351Modulator(ClockInterface& clock,
        uint32_t markOffset, uint32_t spaceOffset, uint32_t holdMs);
    virtual ~Si5351Modulator();

    void setBaseFreq(uint32_t freqHz);
    uint32_t getBaseFreq() const { return _baseFreq; };
    void enable(bool on);
    void sendCW();
    void sendCQ();

    void sendSilence();
    void sendMark();
    void sendSpace();

private:

    ClockInterface& _clock;
    uint32_t _baseFreq = 0;
    uint32_t _markOffset;
    uint32_t _spaceOffset;
    uint32_t _holdMs;
    uint32_t _correction = 600;
};

}

#endif

