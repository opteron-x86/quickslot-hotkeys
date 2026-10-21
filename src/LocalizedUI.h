#pragma once
#include "Localization.h"
#include <imgui.h>
#include <cstdarg>

namespace quickslot::UI {
inline bool Button(const char* label,ImVec2 size={}){return ImGui::Button(translatedLabel(label).c_str(),size);}
inline bool Checkbox(const char* label,bool* value){return ImGui::Checkbox(translatedLabel(label).c_str(),value);}
inline bool MenuItem(const char* label){return ImGui::MenuItem(translatedLabel(label).c_str());}
inline bool CollapsingHeader(const char* label,ImGuiTreeNodeFlags flags=0){return ImGui::CollapsingHeader(translatedLabel(label).c_str(),flags);}
inline bool RadioButton(const char* label,int* value,int button){return ImGui::RadioButton(translatedLabel(label).c_str(),value,button);}
inline bool RadioButton(const char* label,bool active){return ImGui::RadioButton(translatedLabel(label).c_str(),active);}
inline bool Combo(const char* label,int* value,const char* choices,int height=-1){const auto items=translatedChoices(choices);return ImGui::Combo(translatedLabel(label).c_str(),value,items.c_str(),height);}
inline bool SliderFloat(const char* label,float* value,float low,float high,const char* format="%.3f",ImGuiSliderFlags flags=0){return ImGui::SliderFloat(translatedLabel(label).c_str(),value,low,high,tr(format),flags);}
inline bool InputTextWithHint(const char* label,const char* hint,char* buffer,std::size_t size,ImGuiInputTextFlags flags=0){return ImGui::InputTextWithHint(translatedLabel(label).c_str(),tr(hint),buffer,size,flags);}
inline void Text(const char* format,...){va_list args;va_start(args,format);ImGui::TextV(tr(format),args);va_end(args);}
inline void TextWrapped(const char* format,...){va_list args;va_start(args,format);ImGui::TextWrappedV(tr(format),args);va_end(args);}
inline void TextDisabled(const char* format,...){va_list args;va_start(args,format);ImGui::TextDisabledV(tr(format),args);va_end(args);}
inline void SetTooltip(const char* format,...){va_list args;va_start(args,format);ImGui::SetTooltipV(tr(format),args);va_end(args);}
}
