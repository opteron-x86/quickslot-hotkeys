#pragma once
#include "KeyGlyphs.h"
#include <filesystem>
#include <span>
#include <vector>

namespace quickslot {
struct ImageAsset {
    int width=0,height=0;
    std::vector<unsigned char> pixels;
    explicit operator bool()const{return !pixels.empty();}
};
ImageAsset loadImageAsset(const std::filesystem::path& path);
ImageAsset decodeImageAsset(std::span<const unsigned char> bytes,int maxDimension=8192);
struct KeyAtlas {ImageAsset image;KeyGlyphs glyphs{};unsigned loaded=0;};
KeyAtlas loadKeyAtlas(const std::filesystem::path& directory);
}
