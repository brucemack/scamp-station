#ifndef _EditorState_h
#define _EditorState_h

namespace scamp {

class EditorState {
public:

    EditorState(char* editSpace, uint16_t editSpaceSize);

    void addChar(char a);
    void keyBackspace();

    void render() const;
    void clear();

private:

    char* _editSpace;
    uint16_t _editSpaceSize;
    uint16_t _cursor;
};

}

#endif

