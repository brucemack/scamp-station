#include <iostream>
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
}

void EditorState::render() const {
    cout << "RENDER: " << _editSpace << endl;
}


}
