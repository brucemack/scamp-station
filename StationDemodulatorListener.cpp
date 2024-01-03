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
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>

#include "StationDemodulatorListener.h"

using namespace std;

namespace scamp {

StationDemodulatorListener::StationDemodulatorListener(queue_t* rxQueue) 
:   _rxQueue(rxQueue),
    _locked(false),
    _lockedMarkFreq(0),
    _lockedSpaceFreq(0) {
}

void StationDemodulatorListener::frequencyLocked(uint16_t markFreq, uint16_t spaceFreq) {
    //cout << "[Locked on frequency " << markFreq << "/" << spaceFreq << "]" << endl;
    _locked = true;
    _lockedMarkFreq = markFreq;
    _lockedSpaceFreq = spaceFreq;
}

void StationDemodulatorListener::dataSyncAcquired() {
    //cout << endl << "[DATA SYNC]" << endl;
}

void StationDemodulatorListener::badFrameReceived(uint32_t rawFrame) {
    //cout << endl << "[BAD FRAME]" << endl;
}

void StationDemodulatorListener::received(char asciiChar) {
    //cout << endl << "CHAR: " << asciiChar << endl;
    bool added = queue_try_add(_rxQueue, &asciiChar);
}

void StationDemodulatorListener::receivedBit(bool bit, uint16_t frameBitPos, int syncFrameCorr) {
    //cout << " [" << frameBitPos << "]=" << (int)bit;
    //if (frameBitPos == 0) {
    //    cout << "|";
    //}
    //cout << (int)bit;
    //cout.flush();
}

void StationDemodulatorListener::sampleMetrics(uint8_t activeSymbol, bool capture, 
    int32_t lastPLLError,
    float* symbolCorr, float corrThreshold, float corrDiff, float sample) {
    /*
    _sampleCount++;

    if (_logTrigger && 
        _sampleCount >= _logStart && (_sampleCount - _logStart) < _logLen) {
        cout << _sampleCount << " " << activeSymbol << " " << capture << " "
            << lastPLLError << " | " << symbolCorr[1] << " " << symbolCorr[0] << " | "
            << corrThreshold << " " << corrDiff << " [ " << sample << " ]" << endl;
    }
    */
}

}
