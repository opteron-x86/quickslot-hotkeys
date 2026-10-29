#pragma once
#include "ItemIcons.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct ID3D11Device;
namespace quickslot {
class PNGIIIcons {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    using ReadResource=std::function<std::vector<unsigned char>(const std::string&)>;
    using Log=std::function<void(std::string_view)>;
    using UITask=std::function<void(std::function<void()>)>;
    PNGIIIcons(ID3D11Device*,ReadResource,Log,UITask);
    ~PNGIIIcons();
    void beginFrame(bool enabled,std::uint64_t generation);
    std::optional<ItemIcon> find(std::uint32_t form,std::string_view label);
};
}
