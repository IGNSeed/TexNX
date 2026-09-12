#pragma once

#include "texnx/textures/TexturePack.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace texnx::textures {

enum class TextureScanState {
    Ready,
    Error,
};

enum class CurrentTextureState {
    Default,
    KnownPack,
    ExternalOrUnknown,
    Error,
};

struct CurrentTextureResult {
    CurrentTextureState state{CurrentTextureState::Error};
    std::size_t matchedPackIndex{std::numeric_limits<std::size_t>::max()};
    int posixError{0};
    std::uint32_t nativeResult{0};
    std::string errorPath;
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

    // 実際のMinecraft Commonと一覧sort済みpackのfingerprintを照合する。
    [[nodiscard]] static CurrentTextureResult detectCurrentState(
        const std::vector<TexturePack>& packs) noexcept;
};

} // namespace texnx::textures
