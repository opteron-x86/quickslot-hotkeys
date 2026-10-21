#include "UiProbe.h"
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
namespace {std::unordered_map<ImGuiID,probe::Item> items;std::vector<ImGuiID> order;}
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx,ImGuiID id,const ImRect& bb,const ImGuiLastItemData* data) {
    if(!items.contains(id))order.push_back(id);
    auto& item=items[id];item.id=id;item.rect=bb;item.window=ctx->CurrentWindow?ctx->CurrentWindow->Name:"";
    item.scope=ctx->CurrentWindow&&!ctx->CurrentWindow->IDStack.empty()?ctx->CurrentWindow->IDStack.back():0;
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

    std::vector<Item> result;for(const auto id:order)if((items[id].label==label||ImHashStr(label.c_str(),0,items[id].scope)==id)&&std::none_of(result.begin(),result.end(),[&](const auto& item){return item.id==id;}))result.push_back(items[id]);
    return result;
}
Item one(const std::string& label,std::size_t index) {const auto candidates=find(label);if(index>=candidates.size())throw std::runtime_error("UI item not found: "+label);return candidates[index];}
}
