#pragma once
#include <atomic>
struct MockWindow {std::atomic<bool> open{false},blocking{true};};
struct MockSMF {
    MockWindow window;
    bool otherBlocking=false,click=false,disabled=false;
    unsigned registrations=0,texts=0;
    void(__stdcall* render)()=nullptr;
    char path[128]{};
};
