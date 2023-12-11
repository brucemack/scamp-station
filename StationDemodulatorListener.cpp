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
#include <cstring>
#include <iostream>
#include <sstream>

using namespace std;

namespace scamp {

StationDemodulatorListener::StationDemodulatorListener(HD44780* display) 
:   _display(display) {
}

void StationDemodulatorListener::frequencyLocked(uint16_t markFreq, uint16_t spaceFreq) {
    cout << "LOCKED " << markFreq << endl;
    _logTrigger = true;
    _sampleCount = 0;
}

void StationDemodulatorListener::dataSyncAcquired() {
    //cout << endl << "[DATA SYNC]" << endl;
}

void StationDemodulatorListener::badFrameReceived(uint32_t rawFrame) {
    cout << endl << "[BAD FRAME]" << endl;
}

void StationDemodulatorListener::received(char asciiChar) {
    //cout << endl << "CHAR: " << asciiChar << endl;
    cout << asciiChar;
    cout.flush();

    if (_rxSpaceUsed == 80) {
        // Shift down by 20 characters
        for (uint16_t i = 0; i < 60; i++) {
            _rxSpace[i] = _rxSpace[i + 20];
        }
        _rxSpaceUsed = 60;
    }
    _rxSpace[_rxSpaceUsed++] = asciiChar;
    _isDirty = true;
}

void StationDemodulatorListener::receivedBit(bool bit, uint16_t frameBitPos, int syncFrameCorr) {
    //cout << " [" << frameBitPos << "]=" << (int)bit;
    //if (frameBitPos == 0) {
    //    cout << "|";
    //}
    //cout << (int)bit;
    //cout.flush();
}

bool StationDemodulatorListener::isDirty() {
    bool r = _isDirty;
    _isDirty = false;
    return r;
}

void StationDemodulatorListener::render(HD44780& display) const {
    display.clearDisplay();
    display.writeLinear(HD44780::Format::FMT_20x4, 
        (const uint8_t*)_rxSpace, _rxSpaceUsed, 0);
}

void StationDemodulatorListener::sampleMetrics(uint8_t activeSymbol, bool capture, 
    int32_t lastPLLError,
    float* symbolCorr, float corrThreshold, float corrDiff, float sample) {
    
    _sampleCount++;

    if (_logTrigger && 
        _sampleCount >= _logStart && (_sampleCount - _logStart) < _logLen) {
        cout << _sampleCount << " " << activeSymbol << " " << capture << " "
            << lastPLLError << " | " << symbolCorr[1] << " " << symbolCorr[0] << " | "
            << corrThreshold << " " << corrDiff << " [ " << sample << " ]" << endl;
    }
}

}
