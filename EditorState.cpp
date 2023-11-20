#include <iostream>
#include <cstring>

#include "EditorState.h"

using namespace std;

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

void EditorState::render(HD44780& display) const {
    //cout << "RENDER: " << _editSpace << endl;
    display.clearDisplay();
    int l = strlen(_editSpace);
    display.writeLinear(HD44780::Format::FMT_20x4, (const uint8_t*)_editSpace, l, 0);
    display.setCursorLinear(HD44780::Format::FMT_20x4, _cursor);
}


}
