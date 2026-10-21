#pragma once

namespace quickslot::release {

inline constexpr unsigned candidate=20;
inline constexpr unsigned major=0,minor=9,patch=candidate>>4,revision=candidate&15;
inline constexpr char version[]="0.9.0-rc20";
inline constexpr char title[]="Quickslot 0.9.0 RC20";
}
