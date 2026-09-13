#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <switch.h>

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

// Metadataとfile bytesを既存v1形式へ順番に流す共通fingerprint writer。
class TextureFingerprintBuilder final {
public:
    explicit TextureFingerprintBuilder(
        const TextureTreeSnapshot& snapshot) noexcept;

    TextureFingerprintBuilder(const TextureFingerprintBuilder&) = delete;
    TextureFingerprintBuilder& operator=(
        const TextureFingerprintBuilder&) = delete;

    [[nodiscard]] bool beginEntry(
        const TextureTreeEntry& entry) noexcept;
    [[nodiscard]] bool updateFileBytes(const void* data,
                                       std::size_t size) noexcept;
    [[nodiscard]] bool finish(
        std::array<std::uint8_t, 32>& fingerprint) noexcept;

private:
    Sha256Context context_{};
    const TextureTreeSnapshot* snapshot_{nullptr};
    std::size_t expectedEntries_{0};
    std::size_t processedEntries_{0};
    std::uint64_t expectedFileBytes_{0};
    std::uint64_t processedFileBytes_{0};
    bool fileEntryOpen_{false};
    bool valid_{false};
};

class TextureTree final {
public:
    // Commonをrootとする安全なrelative path一覧を作り、必要なら内容もhashする。
    [[nodiscard]] static TextureTreeSnapshot inspect(
        std::string_view rootPath, bool hashContents) noexcept;

    // 列挙済みsnapshotを再利用し、directoryを再走査せず内容をhashする。
    [[nodiscard]] static TextureTreeSnapshot fingerprint(
        std::string_view rootPath, TextureTreeSnapshot snapshot) noexcept;

    // Full hash候補の絞り込み専用。Known確定にはfingerprintも必要。
    [[nodiscard]] static bool metadataEquivalent(
        const TextureTreeSnapshot& left,
        const TextureTreeSnapshot& right) noexcept;

    [[nodiscard]] static bool fingerprintsEqual(
        const TextureTreeSnapshot& left,
        const TextureTreeSnapshot& right) noexcept;
};

} // namespace texnx::textures
