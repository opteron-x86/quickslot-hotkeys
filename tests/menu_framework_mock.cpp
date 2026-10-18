// Isolated ABI fixture; never installed in Skyrim or the mod package.
#include "MenuFrameworkMock.h"
#include <cstdio>
#include <cstdarg>
MockSMF mock;
#define EXPORT extern "C" __declspec(dllexport)
EXPORT void AddSectionItem(const char* path,void(__stdcall* draw)()) {
    ++mock.registrations;mock.render=draw;std::snprintf(mock.path,sizeof(mock.path),"%s",path);
}
#ifndef QUICK_SLOT_INCOMPLETE_API
struct Vec2 {float x,y;};
EXPORT MockSMF* GetMockState(){return &mock;}
EXPORT MockWindow* GetMainWindow(){return &mock.window;}
EXPORT bool IsAnyBlockingWindowOpened(){return mock.otherBlocking||(mock.window.open&&mock.window.blocking);}
EXPORT void igTextUnformatted(const char*,const char*){++mock.texts;}
EXPORT void igTextWrappedV(const char*,va_list){++mock.texts;}
EXPORT void igSeparator(){}
EXPORT void igBeginDisabled(bool disabled){mock.disabled=disabled;}
EXPORT void igEndDisabled(){mock.disabled=false;}
EXPORT bool igButton(const char*,Vec2){const bool click=mock.click;mock.click=false;return click&&!mock.disabled;}
#endif
