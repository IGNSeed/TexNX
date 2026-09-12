#pragma once

#include <cstdint>
#include <string_view>

namespace texnx::config {

enum class LanguageMode {
    System,
    English,
    Japanese,
};

struct AppConfig {
    LanguageMode language{LanguageMode::System};
};

enum class LoadState {
    Loaded,
    Missing,
    Invalid,
    Error,
};

struct LoadResult {
    AppConfig config{};
    LoadState state{LoadState::Missing};
    int posixError{0};
};

struct SaveResult {
    bool succeeded{false};
    int posixError{0};
    std::uint32_t nativeResult{0};
};

[[nodiscard]] std::string_view toConfigValue(LanguageMode mode) noexcept;
[[nodiscard]] int toSelectionIndex(LanguageMode mode) noexcept;
[[nodiscard]] LanguageMode fromSelectionIndex(int index) noexcept;

class ConfigStore final {
public:
    [[nodiscard]] static LoadResult load() noexcept;
    [[nodiscard]] static SaveResult save(const AppConfig& config) noexcept;
};

} // namespace texnx::config
