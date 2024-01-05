/*
SCAMP Encoder/Decoder
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
#ifndef _StationDemodulatorListener_h
#define _StationDemodulatorListener_h

#include <cstdint>
#include <sstream>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#endif

#include "radlib/scamp/DemodulatorListener.h"

class StationDemodulatorListener : public radlib::DemodulatorListener {
public:

    StationDemodulatorListener(queue_t* rxQueue);

    virtual void frequencyLocked(uint16_t markFreq, uint16_t spaceFreq);
    virtual void dataSyncAcquired();
    virtual void badFrameReceived(uint32_t rawFrame);
    virtual void received(char asciiChar);
    virtual void receivedBit(bool bit, uint16_t frameBitPos, int syncFrameCorr);

    virtual void sampleMetrics(uint8_t activeSymbol, bool capture, 
        int32_t lastPLLError,
        float* symbolCorr, float corrThreshold, float corrDiff,
        float sample);

private:

    queue_t* _rxQueue;

    bool _locked;
    uint16_t _lockedMarkFreq;
    uint16_t _lockedSpaceFreq;
};

#endif
