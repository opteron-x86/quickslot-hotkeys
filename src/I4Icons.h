#pragma once
#include "ItemIcons.h"
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;
namespace quickslot {
void invalidateI4Icons();
class I4Icons {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    I4Icons(ID3D11Device*,ID3D11DeviceContext*);
    ~I4Icons();
    void beginFrame();
    std::optional<ItemIcon> find(const Entry&);
};
}
