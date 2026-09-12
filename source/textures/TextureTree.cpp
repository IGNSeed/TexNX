#include "texnx/textures/TextureTree.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <limits>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include <switch.h>

namespace texnx::textures {
namespace {

constexpr std::size_t HashBufferSize = 128U * 1024U;
constexpr std::array<std::uint8_t, 18> FingerprintHeader{
    'T', 'e', 'x', 'N', 'X', '-', 'C', 'o', 'm', 'm', 'o', 'n', '-', 'v', '1',
    0, 0, 1};

struct PendingDirectory {
    std::string relativePath;
    std::string fullPath;
};

std::uint32_t lastNativeResult() noexcept {
    return static_cast<std::uint32_t>(fsdevGetLastResult());
}

TextureTreeSnapshot failure(const int posixError,
                            const std::string_view path) noexcept {
    TextureTreeSnapshot result;
    result.state = TextureTreeState::Error;
    result.posixError = posixError != 0 ? posixError : EIO;
    result.nativeResult = lastNativeResult();
    try {
        result.errorPath.assign(path);
    } catch (...) {
        result.posixError = ENOMEM;
    }
    return result;
}

bool isSafeComponent(const std::string& name) noexcept {
    return !name.empty() && name != "." && name != ".." &&
           name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos &&
           name.find(':') == std::string::npos;
}

bool appendPath(const std::string& parent, const std::string& child,
                std::string& output) {
    const std::size_t separator =
        !parent.empty() && parent.back() != '/' ? 1U : 0U;
    if (parent.size() > static_cast<std::size_t>(FS_MAX_PATH - 1) ||
        child.size() > static_cast<std::size_t>(FS_MAX_PATH - 1) ||
        parent.size() + separator + child.size() >= FS_MAX_PATH) {
        return false;
    }

    output = parent;
    if (separator != 0) {
        output.push_back('/');
    }
    output.append(child);
    return true;
}

bool increment(std::uint64_t& value) noexcept {
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }
    ++value;
    return true;
}

bool addSize(std::uint64_t& value, const std::uint64_t amount) noexcept {
    if (amount > std::numeric_limits<std::uint64_t>::max() - value) {
        return false;
    }
    value += amount;
    return true;
}

bool bytewiseLess(const std::string& left, const std::string& right) noexcept {
    const auto commonLength = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < commonLength; ++index) {
        const auto leftByte = static_cast<unsigned char>(left[index]);
        const auto rightByte = static_cast<unsigned char>(right[index]);
        if (leftByte != rightByte) {
            return leftByte < rightByte;
        }
    }
    return left.size() < right.size();
}

void hashUint64(Sha256Context& context, const std::uint64_t value) noexcept {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(value >> (index * 8U));
    }
    sha256ContextUpdate(&context, bytes.data(), bytes.size());
}

bool hashFile(Sha256Context& context, const std::string& fullPath,
              const std::uint64_t expectedSize, int& posixError) noexcept {
    errno = 0;
    std::FILE* input = std::fopen(fullPath.c_str(), "rb");
    if (input == nullptr) {
        posixError = errno != 0 ? errno : EIO;
        return false;
    }

    struct stat openedInformation {};
    if (::fstat(::fileno(input), &openedInformation) != 0 ||
        !S_ISREG(openedInformation.st_mode) || openedInformation.st_size < 0 ||
        static_cast<std::uint64_t>(openedInformation.st_size) != expectedSize) {
        posixError = errno != 0 ? errno : EIO;
        std::fclose(input);
        return false;
    }

    std::array<unsigned char, HashBufferSize> buffer{};
    std::uint64_t totalRead = 0;
    bool succeeded = true;
    while (true) {
        errno = 0;
        const std::size_t bytesRead =
            std::fread(buffer.data(), 1, buffer.size(), input);
        if (bytesRead > 0) {
            if (!addSize(totalRead, static_cast<std::uint64_t>(bytesRead)) ||
                totalRead > expectedSize) {
                posixError = EOVERFLOW;
                succeeded = false;
                break;
            }
            sha256ContextUpdate(&context, buffer.data(), bytesRead);
        }
        if (bytesRead < buffer.size()) {
            if (std::ferror(input) != 0) {
                posixError = errno != 0 ? errno : EIO;
                succeeded = false;
            }
            if (std::feof(input) != 0 || !succeeded) {
                break;
            }
            if (bytesRead == 0) {
                posixError = EIO;
                succeeded = false;
                break;
            }
        }
    }

    if (succeeded && totalRead != expectedSize) {
        posixError = EIO;
        succeeded = false;
    }
    if (std::fclose(input) != 0 && succeeded) {
        posixError = errno != 0 ? errno : EIO;
        succeeded = false;
    }
    return succeeded;
}

TextureTreeSnapshot hashSnapshot(std::string_view rootPath,
                                 TextureTreeSnapshot snapshot) {
    Sha256Context context{};
    sha256ContextCreate(&context);
    sha256ContextUpdate(&context, FingerprintHeader.data(),
                        FingerprintHeader.size());
    hashUint64(context, static_cast<std::uint64_t>(snapshot.entries.size()));
    hashUint64(context, snapshot.totalFiles);
    hashUint64(context, snapshot.totalDirectories);
    hashUint64(context, snapshot.totalBytes);

    const std::string root(rootPath);
    for (const auto& entry : snapshot.entries) {
        const std::uint8_t type =
            entry.type == TextureTreeEntryType::Directory ? 'D' : 'F';
        sha256ContextUpdate(&context, &type, sizeof(type));
        hashUint64(context,
                   static_cast<std::uint64_t>(entry.relativePath.size()));
        sha256ContextUpdate(&context, entry.relativePath.data(),
                            entry.relativePath.size());
        hashUint64(context, entry.size);

        if (entry.type == TextureTreeEntryType::File) {
            std::string fullPath;
            if (!appendPath(root, entry.relativePath, fullPath)) {
                return failure(ENAMETOOLONG, root);
            }
            int readError = 0;
            if (!hashFile(context, fullPath, entry.size, readError)) {
                return failure(readError, fullPath);
            }
        }
    }

    sha256ContextGetHash(&context, snapshot.fingerprint.data());
    return snapshot;
}

} // namespace

TextureTreeSnapshot TextureTree::inspect(const std::string_view rootPath,
                                         const bool hashContents) noexcept {
    if (rootPath.empty() || rootPath.size() >= FS_MAX_PATH) {
        return failure(rootPath.empty() ? EINVAL : ENAMETOOLONG, rootPath);
    }

    try {
        const std::string root(rootPath);
        errno = 0;
        struct stat rootInformation {};
        if (::lstat(root.c_str(), &rootInformation) != 0) {
            const int rootError = errno;
            if (rootError == ENOENT || rootError == ENOTDIR) {
                TextureTreeSnapshot missing;
                missing.state = TextureTreeState::Missing;
                missing.posixError = rootError;
                missing.nativeResult = lastNativeResult();
                missing.errorPath = root;
                return missing;
            }
            return failure(rootError, root);
        }
        if (!S_ISDIR(rootInformation.st_mode)) {
            return failure(ENOTDIR, root);
        }

        TextureTreeSnapshot result;
        result.state = TextureTreeState::Ready;
        std::vector<PendingDirectory> pending{{{}, root}};

        while (!pending.empty()) {
            PendingDirectory current = std::move(pending.back());
            pending.pop_back();

            errno = 0;
            std::unique_ptr<DIR, decltype(&::closedir)> directory(
                ::opendir(current.fullPath.c_str()), &::closedir);
            if (!directory) {
                return failure(errno, current.fullPath);
            }

            bool enumerationFailed = false;
            int enumerationError = 0;
            std::string enumerationPath;
            while (true) {
                errno = 0;
                dirent* directoryEntry = ::readdir(directory.get());
                if (directoryEntry == nullptr) {
                    if (errno != 0) {
                        enumerationFailed = true;
                        enumerationError = errno;
                        enumerationPath = current.fullPath;
                    }
                    break;
                }

                const std::string name(directoryEntry->d_name);
                if (name == "." || name == "..") {
                    continue;
                }
                if (!isSafeComponent(name)) {
                    enumerationFailed = true;
                    enumerationError = EINVAL;
                    enumerationPath = current.fullPath;
                    break;
                }

                std::string relativePath;
                std::string fullPath;
                if (!appendPath(current.relativePath, name, relativePath) ||
                    !appendPath(current.fullPath, name, fullPath)) {
                    enumerationFailed = true;
                    enumerationError = ENAMETOOLONG;
                    enumerationPath = current.fullPath;
                    break;
                }

                errno = 0;
                struct stat information {};
                if (::lstat(fullPath.c_str(), &information) != 0) {
                    enumerationFailed = true;
                    enumerationError = errno;
                    enumerationPath = fullPath;
                    break;
                }

                if (S_ISDIR(information.st_mode)) {
                    if (!increment(result.totalDirectories)) {
                        enumerationFailed = true;
                        enumerationError = EOVERFLOW;
                        enumerationPath = fullPath;
                        break;
                    }
                    result.entries.push_back(
                        {TextureTreeEntryType::Directory, relativePath, 0});
                    pending.push_back({relativePath, fullPath});
                } else if (S_ISREG(information.st_mode) &&
                           information.st_size >= 0) {
                    const auto fileSize =
                        static_cast<std::uint64_t>(information.st_size);
                    if (!increment(result.totalFiles) ||
                        !addSize(result.totalBytes, fileSize)) {
                        enumerationFailed = true;
                        enumerationError = EOVERFLOW;
                        enumerationPath = fullPath;
                        break;
                    }
                    result.entries.push_back(
                        {TextureTreeEntryType::File, relativePath, fileSize});
                } else {
                    enumerationFailed = true;
                    enumerationError = ENOTSUP;
                    enumerationPath = fullPath;
                    break;
                }
            }

            errno = 0;
            const int closeResult = ::closedir(directory.release());
            if (enumerationFailed) {
                return failure(enumerationError, enumerationPath);
            }
            if (closeResult != 0) {
                return failure(errno, current.fullPath);
            }
        }

        std::sort(result.entries.begin(), result.entries.end(),
                  [](const TextureTreeEntry& left,
                     const TextureTreeEntry& right) {
                      if (left.relativePath != right.relativePath) {
                          return bytewiseLess(left.relativePath,
                                              right.relativePath);
                      }
                      return left.type < right.type;
                  });

        return hashContents ? hashSnapshot(rootPath, std::move(result))
                            : result;
    } catch (...) {
        return failure(ENOMEM, rootPath);
    }
}

bool TextureTree::fingerprintsEqual(
    const TextureTreeSnapshot& left,
    const TextureTreeSnapshot& right) noexcept {
    return left.state == TextureTreeState::Ready &&
           right.state == TextureTreeState::Ready &&
           left.totalFiles == right.totalFiles &&
           left.totalDirectories == right.totalDirectories &&
           left.totalBytes == right.totalBytes &&
           left.fingerprint == right.fingerprint;
}

} // namespace texnx::textures
