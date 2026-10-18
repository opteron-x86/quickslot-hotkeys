#include "UiProbe.h"
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
namespace {std::unordered_map<ImGuiID,probe::Item> items;std::vector<ImGuiID> order;}
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx,ImGuiID id,const ImRect& bb,const ImGuiLastItemData* data) {
    auto& item=items[id];item.id=id;item.rect=bb;item.window=ctx->CurrentWindow?ctx->CurrentWindow->Name:"";
    item.disabled=data&&(data->ItemFlags&ImGuiItemFlags_Disabled);
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags) {
    auto& item=items[id];item.id=id;item.label=label;order.push_back(id);
}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...){}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID id){return items.contains(id)?items[id].label.c_str():nullptr;}
namespace probe {
void clear(){items.clear();order.clear();}
std::vector<Item> find(const std::string& label) {
    std::vector<Item> result;for(const auto id:order)if(items[id].label==label&&std::none_of(result.begin(),result.end(),[&](const auto& item){return item.id==id;}))result.push_back(items[id]);
    // BeginCombo emits ItemAdd but not ItemInfo in this ImGui version.
    if(result.empty())for(const auto& [id,item]:items)if(auto* window=ImGui::FindWindowByName(item.window.c_str());window&&!window->IDStack.empty()&&window->GetID(label.c_str())==id)result.push_back(item);
    return result;
}
Item one(const std::string& label,std::size_t index) {const auto candidates=find(label);if(index>=candidates.size())throw std::runtime_error("UI item not found: "+label);return candidates[index];}
}
