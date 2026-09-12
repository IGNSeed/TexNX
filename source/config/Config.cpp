#include "texnx/config/Config.hpp"

#include "texnx/Paths.hpp"
#include "texnx/filesystem/FileSystem.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string>

#include <borealis/extern/nlohmann/json.hpp>
#include <switch.h>

namespace texnx::config {
namespace {

constexpr std::size_t MaximumConfigBytes = 4096;

LanguageMode languageFromConfigValue(const std::string_view value, bool& valid) noexcept {
    valid = true;
    if (value == "system") {
        return LanguageMode::System;
    }
    if (value == "en-US") {
        return LanguageMode::English;
    }
    if (value == "ja-JP") {
        return LanguageMode::Japanese;
    }

    valid = false;
    return LanguageMode::System;
}

SaveResult failureResult(const int posixError) noexcept {
    return {false, posixError, static_cast<std::uint32_t>(fsdevGetLastResult())};
}

} // namespace

std::string_view toConfigValue(const LanguageMode mode) noexcept {
    switch (mode) {
        case LanguageMode::System:
            return "system";
        case LanguageMode::English:
            return "en-US";
        case LanguageMode::Japanese:
            return "ja-JP";
    }

    return "system";
}

int toSelectionIndex(const LanguageMode mode) noexcept {
    switch (mode) {
        case LanguageMode::System:
            return 0;
        case LanguageMode::English:
            return 1;
        case LanguageMode::Japanese:
            return 2;
    }

    return 0;
}

LanguageMode fromSelectionIndex(const int index) noexcept {
    switch (index) {
        case 1:
            return LanguageMode::English;
        case 2:
            return LanguageMode::Japanese;
        default:
            return LanguageMode::System;
    }
}

LoadResult ConfigStore::load() noexcept {
    errno = 0;
    std::ifstream input(paths::ConfigFile, std::ios::binary);
    if (!input.is_open()) {
        const int openError = errno;
        return {{}, openError == ENOENT ? LoadState::Missing : LoadState::Error,
                openError};
    }

    std::array<char, MaximumConfigBytes + 1> buffer{};
    input.read(buffer.data(), static_cast<std::streamsize>(MaximumConfigBytes + 1));
    const auto bytesRead = static_cast<std::size_t>(input.gcount());
    if (bytesRead == 0 || bytesRead > MaximumConfigBytes || input.bad()) {
        return {{}, LoadState::Invalid, 0};
    }

    const std::string contents(buffer.data(), bytesRead);
    const auto document = nlohmann::json::parse(contents, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        return {{}, LoadState::Invalid, 0};
    }

    const auto language = document.find("language");
    if (language == document.end() || !language->is_string()) {
        return {{}, LoadState::Invalid, 0};
    }

    bool valid = false;
    const auto mode = languageFromConfigValue(language->get_ref<const std::string&>(), valid);
    if (!valid) {
        return {{}, LoadState::Invalid, 0};
    }

    return {{mode}, LoadState::Loaded, 0};
}

SaveResult ConfigStore::save(const AppConfig& config) noexcept {
    const auto directory = filesystem::FileSystem::createDirectory(paths::TexNxDirectory);
    if (!directory.succeeded) {
        return {false, directory.posixError, directory.nativeResult};
    }

    const nlohmann::json document{{"language", toConfigValue(config.language)}};
    errno = 0;
    std::ofstream output(paths::ConfigTemporaryFile,
                         std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return failureResult(errno);
    }

    output << document.dump(2) << '\n';
    output.flush();
    if (!output.good()) {
        const int writeError = errno != 0 ? errno : EIO;
        output.close();
        std::remove(paths::ConfigTemporaryFile);
        return failureResult(writeError);
    }
    output.close();

    errno = 0;
    if (std::rename(paths::ConfigTemporaryFile, paths::ConfigFile) != 0) {
        const int renameError = errno;
        if ((renameError != EEXIST && renameError != ENOTEMPTY) ||
            std::remove(paths::ConfigFile) != 0 ||
            std::rename(paths::ConfigTemporaryFile, paths::ConfigFile) != 0) {
            const int finalError = errno != 0 ? errno : renameError;
            std::remove(paths::ConfigTemporaryFile);
            return failureResult(finalError);
        }
    }

    const Result commitResult = fsdevCommitDevice("sdmc");
    if (R_FAILED(commitResult)) {
        return {false, EIO, static_cast<std::uint32_t>(commitResult)};
    }

    return {true, 0, 0};
}

} // namespace texnx::config
