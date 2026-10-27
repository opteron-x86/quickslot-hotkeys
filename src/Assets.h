#pragma once
#include "KeyGlyphs.h"
#include <filesystem>
#include <vector>

namespace quickslot {
struct ImageAsset {
    int width=0,height=0;
    std::vector<unsigned char> pixels;
    explicit operator bool()const{return !pixels.empty();}
};
ImageAsset loadImageAsset(const std::filesystem::path& path);
struct KeyAtlas {ImageAsset image;KeyGlyphs glyphs{};unsigned loaded=0;};
KeyAtlas loadKeyAtlas(const std::filesystem::path& directory);
}
