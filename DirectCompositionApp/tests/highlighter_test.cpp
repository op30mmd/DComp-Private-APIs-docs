#include "../TreeSitterHighlighter.h"
#include <iostream>
#include <cassert>
#include <cstring>

void test_initialization() {
    TreeSitterHighlighter highlighter;
    bool ok = highlighter.Initialize();
    assert(ok);
    std::cout << "test_initialization passed" << std::endl;
}

void test_cpp_highlighting() {
    TreeSitterHighlighter highlighter;
    highlighter.Initialize();

    const char* code = "int main() { return 0; }";
    highlighter.UpdateSource(code, (uint32_t)strlen(code));

    const auto& highlights = highlighter.GetHighlights();
    assert(!highlights.empty());

    bool found_return = false;
    bool found_int = false;
    for (const auto& h : highlights) {
        if (strcmp(h.captureName, "keyword") == 0) {
            if (h.startByte == 13 && h.endByte == 19) found_return = true;
        }
        if (strcmp(h.captureName, "type") == 0) {
            if (h.startByte == 0 && h.endByte == 3) found_int = true;
        }
    }

    assert(found_return);
    assert(found_int);
    std::cout << "test_cpp_highlighting passed" << std::endl;
}

int main() {
    test_initialization();
    test_cpp_highlighting();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
