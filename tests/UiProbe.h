#pragma once
#include <imgui_internal.h>
#include <string>
#include <vector>
namespace probe {
struct Item {ImGuiID id=0;ImRect rect;std::string label,window;bool disabled=false;};
void clear();
std::vector<Item> find(const std::string& label);
Item one(const std::string& label,std::size_t index=0);
}
