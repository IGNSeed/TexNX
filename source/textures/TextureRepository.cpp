#include "texnx/textures/TextureRepository.hpp"

#include "texnx/Paths.hpp"
#include "texnx/filesystem/FileSystem.hpp"
#include "texnx/textures/TextureTree.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <switch.h>

namespace texnx::textures {
namespace {

constexpr std::size_t DescriptionMaxBytes = 512;

std::string joinPath(const std::string_view parent,
                     const std::string_view child) {
    std::string result;
    result.reserve(parent.size() + child.size() + 1);
    result.append(parent);
    if (!result.empty() && result.back() != '/') {
        result.push_back('/');
    }
    result.append(child);
    return result;
}

unsigned char foldAscii(const unsigned char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<unsigned char>(character + ('a' - 'A'));
    }
    return character;
}

bool packNameLess(const TexturePack& left, const TexturePack& right) noexcept {
    const auto commonLength = std::min(left.name.size(), right.name.size());
    for (std::size_t index = 0; index < commonLength; ++index) {
        const auto leftCharacter =
            foldAscii(static_cast<unsigned char>(left.name[index]));
        const auto rightCharacter =
            foldAscii(static_cast<unsigned char>(right.name[index]));
        if (leftCharacter != rightCharacter) {
            return leftCharacter < rightCharacter;
        }
    }
    if (left.name.size() != right.name.size()) {
        return left.name.size() < right.name.size();
    }
    return left.name < right.name;
}

bool isContinuationByte(const unsigned char value) noexcept {
    return (value & 0xC0U) == 0x80U;
}

bool sanitizeUtf8(std::string& value, const bool truncated) {
    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xEFU &&
        static_cast<unsigned char>(value[1]) == 0xBBU &&
        static_cast<unsigned char>(value[2]) == 0xBFU) {
        value.erase(0, 3);
    }

    std::size_t index = 0;
    while (index < value.size()) {
        const auto first = static_cast<unsigned char>(value[index]);
        if (first < 0x80U) {
            if ((first < 0x20U && first != '\t') || first == 0x7FU) {
                return false;
            }
            ++index;
            continue;
        }

        std::size_t length = 0;
        std::uint32_t codePoint = 0;
        if (first >= 0xC2U && first <= 0xDFU) {
            length = 2;
            codePoint = first & 0x1FU;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            length = 3;
            codePoint = first & 0x0FU;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            length = 4;
            codePoint = first & 0x07U;
        } else {
            return false;
        }

        if (index + length > value.size()) {
            if (truncated) {
                value.resize(index);
                break;
            }
            return false;
        }

        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto continuation =
                static_cast<unsigned char>(value[index + offset]);
            if (!isContinuationByte(continuation)) {
                return false;
            }
            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }

        if ((length == 3 && codePoint < 0x800U) ||
            (length == 4 && codePoint < 0x10000U) ||
            (codePoint >= 0xD800U && codePoint <= 0xDFFFU) ||
            codePoint > 0x10FFFFU) {
            return false;
        }
        index += length;
    }

    if (truncated && !value.empty()) {
        value.append("…");
    }
    return !value.empty() &&
           std::any_of(value.begin(), value.end(), [](const unsigned char value) {
               return value > 0x20U;
           });
}

std::string readDescription(const std::string& commonPath) {
    const auto descriptionPath = joinPath(commonPath, "res/description.txt");
    auto result = filesystem::FileSystem::readFirstLine(
        descriptionPath.c_str(), DescriptionMaxBytes);
    if (!result.succeeded || !sanitizeUtf8(result.value, result.truncated)) {
        return {};
    }
    return result.value;
}

filesystem::OperationResult ensureTextureDirectory() noexcept {
    const auto applicationDirectory =
        filesystem::FileSystem::createDirectory(paths::TexNxDirectory);
    if (!applicationDirectory.succeeded) {
        return applicationDirectory;
    }
    return filesystem::FileSystem::createDirectory(paths::TextureDirectory);
}

TextureScanResult errorResult(const int posixError,
                              const std::uint32_t nativeResult) {
    return {TextureScanState::Error, {}, posixError, nativeResult};
}

} // namespace

TextureScanResult TextureRepository::scan() noexcept {
    const auto directoryResult = ensureTextureDirectory();
    if (!directoryResult.succeeded) {
        return errorResult(directoryResult.posixError,
                           directoryResult.nativeResult);
    }

    errno = 0;
    std::unique_ptr<DIR, decltype(&::closedir)> directory(
        ::opendir(paths::TextureDirectory), &::closedir);
    if (!directory) {
        return errorResult(errno,
                           static_cast<std::uint32_t>(fsdevGetLastResult()));
    }

    TextureScanResult result;
    result.state = TextureScanState::Ready;

    try {
        while (true) {
            errno = 0;
            dirent* entry = ::readdir(directory.get());
            if (entry == nullptr) {
                if (errno != 0) {
                    return errorResult(
                        errno, static_cast<std::uint32_t>(fsdevGetLastResult()));
                }
                break;
            }

            const std::string name(entry->d_name);
            if (name.empty() || name == "." || name == "..") {
                continue;
            }

            std::string checkedName = name;
            if (!sanitizeUtf8(checkedName, false) || checkedName != name) {
                continue;
            }

            const auto rootPath = joinPath(paths::TextureDirectory, name);
            if (filesystem::FileSystem::directoryExists(rootPath.c_str()).state !=
                filesystem::DirectoryState::Found) {
                continue;
            }

            const auto commonPath = joinPath(rootPath, "Common");
            if (filesystem::FileSystem::directoryExists(commonPath.c_str()).state !=
                filesystem::DirectoryState::Found) {
                continue;
            }

            TexturePack pack;
            pack.name = name;
            pack.rootPath = rootPath;
            pack.commonPath = commonPath;
            pack.iconPath = joinPath(commonPath, "res/gui/pack_icon.png");
            pack.description = readDescription(commonPath);
            result.packs.push_back(std::move(pack));
        }

        std::sort(result.packs.begin(), result.packs.end(), packNameLess);
    } catch (...) {
        return errorResult(ENOMEM, 0);
    }

    return result;
}

CurrentTextureResult TextureRepository::detectCurrentState(
    const std::vector<TexturePack>& packs) noexcept {
    const auto installed = TextureTree::inspect(paths::MinecraftCommon, true);
    if (installed.state == TextureTreeState::Missing) {
        CurrentTextureResult result;
        result.state = CurrentTextureState::Default;
        return result;
    }
    if (installed.state == TextureTreeState::Error) {
        return {CurrentTextureState::Error,
                std::numeric_limits<std::size_t>::max(),
                installed.posixError,
                installed.nativeResult,
                installed.errorPath};
    }

    for (std::size_t index = 0; index < packs.size(); ++index) {
        const auto candidate =
            TextureTree::inspect(packs[index].commonPath, true);
        if (candidate.state != TextureTreeState::Ready) {
            return {CurrentTextureState::Error,
                    std::numeric_limits<std::size_t>::max(),
                    candidate.posixError,
                    candidate.nativeResult,
                    candidate.errorPath};
        }
        if (TextureTree::fingerprintsEqual(installed, candidate)) {
            CurrentTextureResult result;
            result.state = CurrentTextureState::KnownPack;
            result.matchedPackIndex = index;
            return result;
        }
    }

    CurrentTextureResult result;
    result.state = CurrentTextureState::ExternalOrUnknown;
    return result;
}

} // namespace texnx::textures
