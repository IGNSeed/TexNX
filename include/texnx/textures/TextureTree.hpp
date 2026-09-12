#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace texnx::textures {

enum class TextureTreeState {
    Ready,
    Missing,
    Error,
};

enum class TextureTreeEntryType {
    Directory,
    File,
};

struct TextureTreeEntry {
    TextureTreeEntryType type{TextureTreeEntryType::File};
    std::string relativePath;
    std::uint64_t size{0};
};

struct TextureTreeSnapshot {
    TextureTreeState state{TextureTreeState::Error};
    std::vector<TextureTreeEntry> entries;
    std::array<std::uint8_t, 32> fingerprint{};
    std::uint64_t totalFiles{0};
    std::uint64_t totalDirectories{0};
    std::uint64_t totalBytes{0};
    int posixError{0};
    std::uint32_t nativeResult{0};
    std::string errorPath;
};

class TextureTree final {
public:
    // Commonをrootとする安全なrelative path一覧を作り、必要なら内容もhashする。
    [[nodiscard]] static TextureTreeSnapshot inspect(
        std::string_view rootPath, bool hashContents) noexcept;

    [[nodiscard]] static bool fingerprintsEqual(
        const TextureTreeSnapshot& left,
        const TextureTreeSnapshot& right) noexcept;
};

} // namespace texnx::textures
