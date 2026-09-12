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

class FileSystem final {
public:
    // path が directory を指すかを読み取り専用で確認する。
    [[nodiscard]] static DirectoryCheckResult directoryExists(const char* path) noexcept;
};

} // namespace texnx::filesystem
