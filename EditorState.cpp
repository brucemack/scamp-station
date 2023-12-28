/*
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
#include <iostream>
#include <cstring>

#include "EditorState.h"

using namespace std;
using namespace radlib;

namespace scamp {

EditorState::EditorState(char* editSpace, uint16_t editSpaceSize) 
:   _editSpace(editSpace),
    _editSpaceSize(editSpaceSize),
    _cursor(0) {
    clear();
}

void EditorState::addChar(char a) {
    if (_cursor < _editSpaceSize) {
        _editSpace[_cursor] = a;
        _cursor++;
    }
}

void EditorState::keyBackspace() {
    if (_cursor > 0) {
        _cursor--;
        _editSpace[_cursor] = 0;
    }
}

void EditorState::clear() {
    for (uint16_t i = 0; i < _editSpaceSize; i++) {
        _editSpace[i] = 0;
    }
    _cursor = 0;
}

bool EditorState::isClear() const {
    return _editSpace[0] == 0;
}

void EditorState::render(HD44780& display) const {
    display.clearDisplay();
    int l = strlen(_editSpace);
    display.writeLinear(HD44780::Format::FMT_20x4, (const uint8_t*)_editSpace, l, 0);
    display.setCursorLinear(HD44780::Format::FMT_20x4, _cursor);
}


}
