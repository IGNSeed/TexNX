#pragma once

#include <cstdint>

namespace texnx::filesystem {

enum class DirectoryState {
    Found,
    NotFound,
    Error,
};

struct DirectoryCheckResult {
    DirectoryState state{DirectoryState::Error};
    int posixError{0};
    std::uint32_t nativeResult{0};
};

struct OperationResult {
    bool succeeded{false};
    int posixError{0};
    std::uint32_t nativeResult{0};
};

class FileSystem final {
public:
    // path が directory を指すかを読み取り専用で確認する。
    [[nodiscard]] static DirectoryCheckResult directoryExists(const char* path) noexcept;

    // 既存 directory は成功として扱い、それ以外の場合だけ作成を試みる。
    [[nodiscard]] static OperationResult createDirectory(const char* path) noexcept;
};

} // namespace texnx::filesystem
