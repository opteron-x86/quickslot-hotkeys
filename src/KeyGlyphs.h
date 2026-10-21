#pragma once
#include "ControllerTypes.h"
#include <array>
#include <string>
namespace quickslot {

inline std::string keyGlyphName(unsigned scan) {
    if(scan>=256)return padGlyphName(scan);
    if(scan>=2&&scan<=10)return std::to_string(scan-1);
    if(scan==11)return "0";
    if(scan>=59&&scan<=68)return "F"+std::to_string(scan-58);
    if(scan==87)return "F11";if(scan==88)return "F12";
    constexpr unsigned letters[]{30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44};
    for(unsigned i=0;i<26;++i)if(scan==letters[i])return std::string(1,char('A'+i));
    switch(scan) {
    case 1:return "Esc";case 12:return "Hyphen";case 13:return "Equal";case 14:return "Backspace";case 15:return "Tab";
    case 26:return "Bracketleft";case 27:return "Bracketright";case 28:return "Enter";case 29:return "L-Ctrl";
    case 39:return "Semicolon";case 40:return "Quotesingle";case 41:return "Tilde";case 42:return "L-Shift";
    case 43:return "Backslash";case 51:return "Comma";case 52:return "Period";case 53:return "Slash";case 54:return "R-Shift";
    case 55:return "NumPadMult";case 56:return "L-Alt";case 57:return "Space";case 58:return "CapsLock";
    case 69:return "NumLock";case 70:return "ScrollLock";
    case 71:return "Keypad7";case 72:return "Keypad8";case 73:return "NumPad9";case 74:return "NumPadMinus";
    case 75:return "Keypad4";case 76:return "Keypad5";case 77:return "Keypad6";case 78:return "NumPadPlus";
    case 79:return "Keypad1";case 80:return "Keypad2";case 81:return "Keypad3";case 82:return "NumPad0";case 83:return "NumPadDec";
    case 156:return "KeypadEnter";case 157:return "R-Ctrl";case 181:return "NumPadDivide";case 183:return "PrintScreen";case 184:return "R-Alt";
    case 197:return "Pause";case 199:return "Home";case 200:return "Up";case 201:return "PgUp";case 203:return "Left";
    case 205:return "Right";case 207:return "End";case 208:return "Down";case 209:return "PgDn";case 210:return "Insert";case 211:return "Delete";
    default:return {};
    }
}
inline std::string keyLabel(unsigned scan) {
    if(scan>=256&&scan<304)return padLabel(PadButton((scan-256)%16+1),(scan-256)/16);
    auto name=keyGlyphName(scan);return name.empty()?"Key "+std::to_string(scan):name;
}
struct KeyGlyph {float u0=0,v0=0,u1=0,v1=0,aspect=0;};
using KeyGlyphs=std::array<KeyGlyph,288>;
}
