#pragma once
#include "ItemIcons.h"
#include <memory>
#include <string_view>

struct ID3D11Device;
struct ID3D11DeviceContext;
namespace quickslot {
void invalidateI4Icons();
std::uint64_t itemIconGeneration();
class I4Icons {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    I4Icons(ID3D11Device*,ID3D11DeviceContext*);
    ~I4Icons();
    // With drawing off, entries still resolve to their I4 labels and no SWF artwork is rendered.
    // artwork: 0 I4's icon for each item, 1 SkyUI's icon for each type, 2 no SWF icons.
    void beginFrame(unsigned artwork);
    std::optional<ItemIcon> find(const Entry&);
    std::optional<std::string_view> label(const Entry&)const;
};
}
