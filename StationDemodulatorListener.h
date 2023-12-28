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

#include "radlib/lcd/HD44780.h"
#include "hello-scamp/DemodulatorListener.h"

namespace scamp {

class StationDemodulatorListener : public DemodulatorListener {
public:

    StationDemodulatorListener(radlib::HD44780* display);

    virtual void frequencyLocked(uint16_t markFreq, uint16_t spaceFreq);
    virtual void dataSyncAcquired();
    virtual void badFrameReceived(uint32_t rawFrame);
    virtual void received(char asciiChar);
    virtual void receivedBit(bool bit, uint16_t frameBitPos, int syncFrameCorr);

    virtual void sampleMetrics(uint8_t activeSymbol, bool capture, 
        int32_t lastPLLError,
        float* symbolCorr, float corrThreshold, float corrDiff,
        float sample);

    /**
     * Returns an indication of whether any data has been received since the 
     * last call.
     */
    bool isDirty();

    void render(radlib::HD44780& display) const;

    void setLogWindow(uint32_t logStart, uint32_t logLen) {
        _logStart = logStart;
        _logLen = logLen;
    }

private:

    radlib::HD44780* _display;
    char _rxSpace[80];
    uint16_t _rxSpaceUsed = 0;
    bool _isDirty = false;
    uint32_t _sampleCount = 0;
    bool _logTrigger = false;
    uint32_t _logStart = 0;
    uint32_t _logLen = 0;
};

}

#endif
