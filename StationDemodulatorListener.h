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

#include "hello-lcd/HD44780.h"
#include "hello-scamp/DemodulatorListener.h"

namespace scamp {

class StationDemodulatorListener : public DemodulatorListener {
public:

    StationDemodulatorListener(HD44780* display);

    virtual void frequencyLocked(uint16_t markFreq, uint16_t spaceFreq);
    virtual void dataSyncAcquired();
    virtual void badFrameReceived(uint32_t rawFrame);
    virtual void received(char asciiChar);

private:

    HD44780* _display;
};

}

#endif
