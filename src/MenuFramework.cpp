#include "MenuFramework.h"
#include "Version.h"
#include <atomic>
#include <cstdint>
#include <cstdarg>
#include <filesystem>
#include <vector>
namespace RE {class InputEvent;}
#include "../third_party/SKSEMenuFramework.h"

namespace quickslot {
namespace {
namespace UI=ImGuiMCP;
using Window=SKSEMenuFramework::Model::WindowInterface;
using Blocking=SKSEMenuFramework::Model::IsAnyBlockingWindowOpenedFuction;
std::atomic<Blocking> blocking{nullptr};
std::atomic<Window*> mainWindow{nullptr};
FrameworkClient client;
bool registered=false;

void __stdcall drawPage() {
    const auto state=client.status();
    UI::TextUnformatted(release::title);
    UI::Separator();
    if(state.ready)UI::TextWrapped("%zu entries · %zu groups",state.entries,state.groups);
    else UI::TextUnformatted("Load a game to edit Quickslot.");
    UI::BeginDisabled(!state.ready||state.editing||state.pending);
    if(UI::Button("Open editor")&&client.requestEditor())mainWindow.load()->IsOpen=false;
    UI::EndDisabled();
    if(state.pending)UI::TextUnformatted("Opening editor…");
    else if(state.editing)UI::TextUnformatted("Editor open");
    if(!state.message.empty())UI::TextWrapped("%s",state.message.c_str());

}
}

bool frameworkCapturingInput() {
    const auto* window=mainWindow.load();const auto query=blocking.load();
    return (window&&window->IsOpen.load())||(query&&query());
}

bool registerMenuFramework(FrameworkClient callbacks) {
    if(registered)return true;
    const auto module=GetModuleHandleW(L"SKSEMenuFramework.dll");
    if(!module||!callbacks.status||!callbacks.requestEditor)return false;
    for(const char* name:{"AddSectionItem","GetMainWindow","IsAnyBlockingWindowOpened",
                         "igTextUnformatted","igTextWrappedV","igSeparator","igBeginDisabled","igEndDisabled","igButton"})
        if(!GetProcAddress(module,name))return false;
    const auto getWindow=GetMenuFrameworkFunction<SKSEMenuFramework::Model::GetMainWindowFunction>("GetMainWindow");
    auto* window=getWindow();if(!window)return false;
    client=callbacks;
    mainWindow.store(window);
    blocking.store(GetMenuFrameworkFunction<Blocking>("IsAnyBlockingWindowOpened"));
    SKSEMenuFramework::SetSection("Quickslot");
    SKSEMenuFramework::AddSectionItem("Equipment & widget",drawPage);
    registered=true;
    return true;
}
}
