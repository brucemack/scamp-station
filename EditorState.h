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
#ifndef _EditorState_h
#define _EditorState_h

#include "radlib/lcd/HD44780.h"

namespace scamp {

class EditorState {
public:

    EditorState(char* editSpace, uint16_t editSpaceSize);

    void addChar(char a);
    void keyBackspace();

    bool isClear() const;
    void render(radlib::HD44780& display) const;
    void clear();

private:

    char* _editSpace;
    uint16_t _editSpaceSize;
    uint16_t _cursor;
};

}

#endif

