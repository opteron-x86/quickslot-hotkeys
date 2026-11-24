#pragma once
#include "KeyGlyphs.h"
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace quickslot {
struct ImageAsset {
    int width=0,height=0;
    std::vector<unsigned char> pixels;
    explicit operator bool()const{return !pixels.empty();}
};
ImageAsset loadImageAsset(const std::filesystem::path& path);
ImageAsset decodeImageAsset(std::span<const unsigned char> bytes,int maxDimension=8192);
// Decodes a PNG or SVG theme asset into straight RGBA pixels, preserving SVG aspect ratio.
ImageAsset decodeThemeImage(std::string_view extension,std::span<const unsigned char> bytes,unsigned svgMaxDimension=512,int maxDimension=2048);
ImageAsset loadThemeImage(const std::filesystem::path& path,unsigned svgMaxDimension=512,int maxDimension=2048);
struct KeyAtlas {ImageAsset image;KeyGlyphs glyphs{};unsigned loaded=0;};
KeyAtlas loadKeyAtlas(const std::filesystem::path& directory);
}
