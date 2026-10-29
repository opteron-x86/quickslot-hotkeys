#pragma once
#include "Assets.h"
#include <string_view>
namespace quickslot {
// Returns a square, straight-RGBA icon with centered artwork.
ImageAsset decodePNGIIImage(std::string_view path,std::span<const unsigned char> bytes);
}
