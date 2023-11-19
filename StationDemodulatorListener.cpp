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
#include "StationDemodulatorListener.h"

#include <cstdint>
#include <iostream>
#include <sstream>

using namespace std;

namespace scamp {

StationDemodulatorListener::StationDemodulatorListener(HD44780* display) 
:   _display(display) {
}

void StationDemodulatorListener::frequencyLocked(uint16_t markFreq, uint16_t spaceFreq) {
    cout << "LOCKED " << markFreq << endl;
}

void StationDemodulatorListener::dataSyncAcquired() {
    cout << "DATA SYNC" << endl;
}

void StationDemodulatorListener::badFrameReceived(uint32_t rawFrame) {
}

void StationDemodulatorListener::received(char asciiChar) {
    cout << "CHAR: " << asciiChar << endl;
    _display->write((uint8_t)asciiChar);
}

void StationDemodulatorListener::receivedBit(bool bit, uint16_t frameBitPos, int syncFrameCorr) {
    cout << "BIT " << (int)bit << endl;    
}


}
