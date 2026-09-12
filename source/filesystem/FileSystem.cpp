#include "texnx/filesystem/FileSystem.hpp"

#include <cerrno>
#include <cstdio>
#include <sys/stat.h>

#include <switch.h>

namespace texnx::filesystem {

DirectoryCheckResult FileSystem::directoryExists(const char* path) noexcept {
    if (path == nullptr || path[0] == '\0') {
        return {DirectoryState::Error, EINVAL, 0};
    }

    errno = 0;
    struct stat information {};
    if (::stat(path, &information) == 0) {
        const bool isDirectory = S_ISDIR(information.st_mode);
        return {isDirectory ? DirectoryState::Found : DirectoryState::NotFound, 0, 0};
    }

    const int posixError = errno;
    const auto nativeResult = static_cast<std::uint32_t>(fsdevGetLastResult());
    if (posixError == ENOENT || posixError == ENOTDIR) {
        return {DirectoryState::NotFound, posixError, nativeResult};
    }

    return {DirectoryState::Error, posixError, nativeResult};
}

OperationResult FileSystem::createDirectory(const char* path) noexcept {
    const auto existing = directoryExists(path);
    if (existing.state == DirectoryState::Found) {
        return {true, 0, 0};
    }
    if (path == nullptr || path[0] == '\0') {
        return {false, EINVAL, 0};
    }

    errno = 0;
    if (::mkdir(path, 0777) == 0) {
        return {true, 0, 0};
    }

    const int posixError = errno;
    if (posixError == EEXIST &&
        directoryExists(path).state == DirectoryState::Found) {
        return {true, 0, 0};
    }
    return {false, posixError,
            static_cast<std::uint32_t>(fsdevGetLastResult())};
}

FirstLineResult FileSystem::readFirstLine(const char* path,
                                          const std::size_t maxBytes) noexcept {
    if (path == nullptr || path[0] == '\0' || maxBytes == 0) {
        return {false, false, {}, EINVAL, 0};
    }

    errno = 0;
    std::FILE* input = std::fopen(path, "rb");
    if (input == nullptr) {
        return {false, false, {}, errno,
                static_cast<std::uint32_t>(fsdevGetLastResult())};
    }

    FirstLineResult result;
    result.succeeded = true;

    try {
        result.value.reserve(maxBytes);
        bool reachedLineEnd = false;
        while (result.value.size() < maxBytes) {
            const int character = std::fgetc(input);
            if (character == EOF) {
                if (std::ferror(input) != 0) {
                    result.succeeded = false;
                    result.posixError = errno != 0 ? errno : EIO;
                    result.nativeResult =
                        static_cast<std::uint32_t>(fsdevGetLastResult());
                }
                break;
            }
            if (character == '\n') {
                reachedLineEnd = true;
                break;
            }
            result.value.push_back(static_cast<char>(character));
        }

        if (result.succeeded && !reachedLineEnd &&
            result.value.size() == maxBytes) {
            const int following = std::fgetc(input);
            if (following != EOF && following != '\n') {
                result.truncated = true;
            } else if (following == EOF && std::ferror(input) != 0) {
                result.succeeded = false;
                result.posixError = errno != 0 ? errno : EIO;
                result.nativeResult =
                    static_cast<std::uint32_t>(fsdevGetLastResult());
            }
        }
    } catch (...) {
        result = {false, false, {}, ENOMEM, 0};
    }

    std::fclose(input);
    if (result.succeeded && !result.value.empty() &&
        result.value.back() == '\r') {
        result.value.pop_back();
    }
    return result;
}

} // namespace texnx::filesystem
