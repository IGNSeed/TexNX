#pragma once

#include "texnx/textures/TexturePack.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace texnx::textures {

enum class TextureInstallStage {
    Preflight,
    Removing,
    Creating,
    Copying,
    Committing,
    Verifying,
    CleaningUp,
};

enum class TextureInstallError {
    None,
    InvalidSource,
    SourcePreflight,
    DestinationPreflight,
    DestinationRemove,
    DestinationCreate,
    SourceRead,
    DestinationWrite,
    Commit,
    Verification,
    Cleanup,
};

struct TextureInstallProgress {
    TextureInstallStage stage{TextureInstallStage::Preflight};
    std::uint64_t completed{0};
    std::uint64_t total{1};
};

struct TextureInstallResult {
    bool succeeded{false};
    TextureInstallStage stage{TextureInstallStage::Preflight};
    TextureInstallError error{TextureInstallError::None};
    int posixError{0};
    std::uint32_t nativeResult{0};
    std::string errorPath;
    std::uint64_t totalFiles{0};
    std::uint64_t totalDirectories{0};
    std::uint64_t totalBytes{0};
    bool destinationTouched{false};
    bool cleanupAttempted{false};
    bool cleanupSucceeded{false};
};

using TextureInstallProgressCallback =
    std::function<void(const TextureInstallProgress&)>;

class TextureInstaller final {
public:
    // source packは変更せず、検査済みCommonだけをMinecraft側へcopyする。
    [[nodiscard]] static TextureInstallResult apply(
        const TexturePack& pack,
        const TextureInstallProgressCallback& progress = {}) noexcept;

    // Minecraft側Commonを完全削除し、game内蔵のDefaultへ戻す。
    [[nodiscard]] static TextureInstallResult restoreDefault(
        const TextureInstallProgressCallback& progress = {}) noexcept;
};

} // namespace texnx::textures
