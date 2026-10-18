#pragma once
#include <cstddef>
#include <string>

namespace quickslot {
struct FrameworkStatus {
    bool ready=false,editing=false,pending=false;
    std::size_t groups=0,entries=0;
    unsigned editorKey=0;
    std::string message;
};
struct FrameworkClient {
    FrameworkStatus(*status)()=nullptr;
    bool(*requestEditor)()=nullptr;
};
bool registerMenuFramework(FrameworkClient);
bool frameworkCapturingInput();
}
