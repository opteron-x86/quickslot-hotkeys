#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace preview {
template<class Interface, std::size_t N>
std::size_t ShaderInstanceCount(const std::array<Interface*, N>&, unsigned reported) noexcept {
    return (std::min)(std::size_t(reported), N);
}

template<class Interface, std::size_t N>
bool UnexpectedShaderInstances(const std::array<Interface*, N>& instances, unsigned reported) noexcept {
    if (reported > N) return true;
    for (std::size_t i = reported; i < N; ++i)
        if (instances[i]) return true;
    return false;
}

template<class Interface, std::size_t N>
void ReleaseShaderInstances(std::array<Interface*, N>& instances, unsigned reported) noexcept {
    // GetShader owns only the returned range, not unused output-array slots.
    for (std::size_t i = 0; i < ShaderInstanceCount(instances, reported); ++i)
        if (auto* instance = std::exchange(instances[i], nullptr)) instance->Release();
}
}
