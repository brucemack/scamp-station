#ifndef _Si5351Modulator_h
#define _Si5351Modulator_h

#include <cstdint>
#include <iostream>

#include "hello-scamp/Modulator.h"

namespace scamp {

class Si5351Modulator : public Modulator {
public:

    Si5351Modulator();
    virtual ~Si5351Modulator();

    void enable(bool on);
    void sendSilence();
    void sendMark();
    void sendSpace();

private:

};

}

#endif

