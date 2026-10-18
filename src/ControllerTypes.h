#pragma once
#include <array>
#include <string>

namespace quickslot {
enum class PadButton : unsigned {none,up,right,down,left,a,b,x,y,lb,rb,lt,rt,ls,rs,back,start,count};
struct PadBinding {
    PadButton button=PadButton::none,modifier=PadButton::none;
    bool operator==(const PadBinding&)const=default;
};
inline constexpr std::array<unsigned,17> padNativeCodes{0,1,8,2,4,0x1000,0x2000,0x4000,0x8000,0x100,0x200,9,10,0x40,0x80,0x20,0x10};
enum class PadCodeSet {xinput,orbis};
inline constexpr std::array<unsigned,17> padOrbisCodes{0,0x10,0x20,0x40,0x80,0x4000,0x2000,0x8000,0x1000,0x400,0x800,9,10,2,4,0x100000,8};
inline constexpr const auto& padCodes(PadCodeSet codeSet) {return codeSet==PadCodeSet::orbis?padOrbisCodes:padNativeCodes;}
inline constexpr PadButton padButton(unsigned native,PadCodeSet codeSet=PadCodeSet::xinput) {
    const auto& codes=padCodes(codeSet);
    for(unsigned i=1;i<codes.size();++i)if(codes[i]==native)return PadButton(i);
    return PadButton::none;
}
using PadState=std::array<bool,unsigned(PadButton::count)>;
inline PadState padPhysicalState(PadCodeSet codeSet,unsigned buttons,unsigned leftTrigger,unsigned rightTrigger) {
    PadState state{};
    const auto& codes=padCodes(codeSet);
    for(unsigned i=1;i<state.size();++i) {
        if(PadButton(i)==PadButton::lt)state[i]=leftTrigger>30;
        else if(PadButton(i)==PadButton::rt)state[i]=rightTrigger>30;
        else state[i]=(buttons&codes[i])!=0;
    }
    return state;
}
inline bool validPadButton(PadButton b){return unsigned(b)<unsigned(PadButton::count);}
inline bool validPadBinding(PadBinding b){return validPadButton(b.button)&&validPadButton(b.modifier)&&(b.button==PadButton::none?b.modifier==PadButton::none:b.button!=b.modifier);}
inline std::string padLabel(PadButton b,unsigned style=0) {
    constexpr const char* xbox[]{"None","D-pad Up","D-pad Right","D-pad Down","D-pad Left","A","B","X","Y","LB","RB","LT","RT","LS","RS","Back","Start"};
    constexpr const char* ps[]{"None","D-pad Up","D-pad Right","D-pad Down","D-pad Left","Cross","Circle","Square","Triangle","L1","R1","L2","R2","L3","R3","Touchpad","Options"};
    constexpr const char* vr[]{"None","None","None","None","None","Main A/X","Main B/Y","Off A/X","Off B/Y","Off grip","Main grip","Off trigger","Main trigger","Off stick","Main stick","Off pad","Main pad"};
    return validPadButton(b)?(style==2?vr:style==1?ps:xbox)[unsigned(b)]:"None";
}
inline std::string padLabel(PadBinding b,unsigned style=0){return b.modifier==PadButton::none?padLabel(b.button,style):padLabel(b.modifier,style)+" + "+padLabel(b.button,style);}
inline unsigned padGlyph(PadButton b,unsigned style){return 256+style*16+unsigned(b)-1;}
inline std::string padGlyphName(unsigned glyph) {
    if(glyph<256||glyph>=288)return {};
    constexpr const char* names[]{"","","","","A","B","X","Y","LB","RB","LT","RT","L3","R3","Back","Start"};
    const auto name=names[(glyph-256)%16];return *name?std::string(glyph<272?"360_":"PS3_")+name:std::string{};
}
}
