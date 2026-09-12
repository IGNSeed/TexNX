#pragma once

#include "texnx/textures/TexturePack.hpp"

#include <cstdint>
#include <vector>

namespace texnx::textures {

enum class TextureScanState {
    Ready,
    Error,
};

enum class CurrentTextureState {
    Default,
    ExternalOrUnknown,
};

struct TextureScanResult {
    TextureScanState state{TextureScanState::Error};
    std::vector<TexturePack> packs;
    int posixError{0};
    std::uint32_t nativeResult{0};
};

class TextureRepository final {
public:
    // Textures directoryを用意し、直下の有効なpackだけを返す。
    [[nodiscard]] static TextureScanResult scan() noexcept;

    // fingerprint実装前は、実際のMinecraft Commonの存在だけをtruthとする。
    [[nodiscard]] static CurrentTextureState detectCurrentState() noexcept;
};

} // namespace texnx::textures
