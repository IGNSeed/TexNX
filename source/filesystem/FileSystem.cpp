#include "texnx/filesystem/FileSystem.hpp"

#include <cerrno>
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

} // namespace texnx::filesystem
