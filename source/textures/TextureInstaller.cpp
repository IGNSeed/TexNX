#include "texnx/textures/TextureInstaller.hpp"

#include "texnx/Paths.hpp"
#include "texnx/filesystem/FileSystem.hpp"
#include "texnx/textures/TextureTree.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <switch.h>

namespace texnx::textures {
namespace {

constexpr std::size_t CopyBufferSize = 128U * 1024U;

std::uint32_t lastNativeResult() noexcept {
    return static_cast<std::uint32_t>(fsdevGetLastResult());
}

void emitProgress(const TextureInstallProgressCallback& callback,
                  const TextureInstallStage stage,
                  const std::uint64_t completed,
                  const std::uint64_t total) noexcept {
    if (!callback) {
        return;
    }
    try {
        callback({stage, completed, total == 0 ? 1 : total});
    } catch (...) {
        // UI通知失敗でfilesystem処理の整合性を変えない。
    }
}

TextureInstallResult failure(const TextureInstallStage stage,
                             const TextureInstallError error,
                             const int posixError,
                             const std::uint32_t nativeResult,
                             const std::string_view path) noexcept {
    TextureInstallResult result;
    result.stage = stage;
    result.error = error;
    result.posixError = posixError != 0 ? posixError : EIO;
    result.nativeResult = nativeResult;
    try {
        result.errorPath.assign(path);
    } catch (...) {
        result.posixError = ENOMEM;
    }
    return result;
}

bool appendPath(const std::string_view parent, const std::string_view child,
                std::string& output) {
    const std::size_t separator =
        !parent.empty() && parent.back() != '/' ? 1U : 0U;
    if (parent.size() >= FS_MAX_PATH || child.size() >= FS_MAX_PATH ||
        parent.size() + separator + child.size() >= FS_MAX_PATH) {
        return false;
    }
    output.assign(parent);
    if (separator != 0) {
        output.push_back('/');
    }
    output.append(child);
    return true;
}

bool isValidPack(const TexturePack& pack) {
    if (pack.name.empty() || pack.name == "." || pack.name == ".." ||
        pack.name.find('/') != std::string::npos ||
        pack.name.find('\\') != std::string::npos ||
        pack.name.find(':') != std::string::npos) {
        return false;
    }

    std::string expectedRoot;
    std::string expectedCommon;
    return appendPath(paths::TextureDirectory, pack.name, expectedRoot) &&
           appendPath(expectedRoot, "Common", expectedCommon) &&
           pack.rootPath == expectedRoot && pack.commonPath == expectedCommon;
}

TextureInstallResult snapshotFailure(const TextureTreeSnapshot& snapshot,
                                     const TextureInstallStage stage,
                                     const TextureInstallError error) noexcept {
    return failure(stage, error, snapshot.posixError, snapshot.nativeResult,
                   snapshot.errorPath);
}

filesystem::OperationResult createDirectoryStrict(const char* path) noexcept {
    if (path == nullptr || path[0] == '\0') {
        return {false, EINVAL, 0};
    }

    errno = 0;
    struct stat information {};
    if (::lstat(path, &information) == 0) {
        return {S_ISDIR(information.st_mode),
                S_ISDIR(information.st_mode) ? 0 : ENOTDIR,
                0};
    }
    const int inspectionError = errno;
    if (inspectionError != ENOENT) {
        return {false, inspectionError, lastNativeResult()};
    }

    errno = 0;
    if (::mkdir(path, 0777) == 0) {
        return {true, 0, 0};
    }
    const int createError = errno;
    const auto nativeResult = lastNativeResult();
    if (createError == EEXIST) {
        errno = 0;
        struct stat existing {};
        if (::lstat(path, &existing) == 0 && S_ISDIR(existing.st_mode)) {
            return {true, 0, 0};
        }
    }
    return {false, createError, nativeResult};
}

bool removeSnapshot(const std::string& root,
                    const TextureTreeSnapshot& snapshot,
                    const TextureInstallProgressCallback& progress,
                    const TextureInstallStage progressStage,
                    TextureInstallResult& errorResult) {
    const std::uint64_t total =
        static_cast<std::uint64_t>(snapshot.entries.size()) + 1U;
    std::uint64_t completed = 0;
    std::uint64_t lastReportedPercent = 0;
    int firstError = 0;
    std::uint32_t firstNativeResult = 0;
    std::string firstErrorPath;

    // 削除開始後にallocation failureで止まらないよう、directory順序を先に確定する。
    std::vector<const TextureTreeEntry*> directories;
    directories.reserve(static_cast<std::size_t>(snapshot.totalDirectories));
    for (const auto& entry : snapshot.entries) {
        if (entry.type == TextureTreeEntryType::Directory) {
            directories.push_back(&entry);
        }
    }
    std::sort(directories.begin(), directories.end(),
              [](const TextureTreeEntry* left,
                 const TextureTreeEntry* right) {
                  if (left->relativePath.size() != right->relativePath.size()) {
                      return left->relativePath.size() >
                             right->relativePath.size();
                  }
                  return left->relativePath > right->relativePath;
              });

    const auto rememberError = [&](const int posixError,
                                   const std::string& path) {
        if (firstError != 0) {
            return;
        }
        firstError = posixError != 0 ? posixError : EIO;
        firstNativeResult = lastNativeResult();
        try {
            firstErrorPath = path;
        } catch (...) {
            firstError = ENOMEM;
        }
    };

    const auto reportProgress = [&] {
        const auto percent = static_cast<std::uint64_t>(
            (static_cast<long double>(completed) * 100.0L) /
            static_cast<long double>(total));
        if (completed == total || percent != lastReportedPercent) {
            lastReportedPercent = percent;
            emitProgress(progress, progressStage, completed, total);
        }
    };

    emitProgress(progress, progressStage, 0, total);
    for (const auto& entry : snapshot.entries) {
        if (entry.type != TextureTreeEntryType::File) {
            continue;
        }
        std::string path;
        if (!appendPath(root, entry.relativePath, path)) {
            rememberError(ENAMETOOLONG, root);
        } else {
            errno = 0;
            if (std::remove(path.c_str()) != 0) {
                rememberError(errno, path);
            }
        }
        ++completed;
        reportProgress();
    }

    for (const TextureTreeEntry* entry : directories) {
        std::string path;
        if (!appendPath(root, entry->relativePath, path)) {
            rememberError(ENAMETOOLONG, root);
        } else {
            errno = 0;
            if (::rmdir(path.c_str()) != 0) {
                rememberError(errno, path);
            }
        }
        ++completed;
        reportProgress();
    }

    errno = 0;
    if (::rmdir(root.c_str()) != 0) {
        rememberError(errno, root);
    }
    ++completed;
    reportProgress();

    if (firstError == 0) {
        return true;
    }
    errorResult = failure(TextureInstallStage::Removing,
                          TextureInstallError::DestinationRemove, firstError,
                          firstNativeResult, firstErrorPath);
    return false;
}

bool ensureDestinationRoot(TextureInstallResult& errorResult) noexcept {
    constexpr std::array<const char*, 5> directories{
        paths::AtmosphereDirectory,
        paths::AtmosphereContentsDirectory,
        paths::MinecraftContentsDirectory,
        paths::MinecraftLayeredFsRoot,
        paths::MinecraftCommon,
    };

    for (const char* directory : directories) {
        const auto created = createDirectoryStrict(directory);
        if (!created.succeeded) {
            errorResult = failure(TextureInstallStage::Creating,
                                  TextureInstallError::DestinationCreate,
                                  created.posixError, created.nativeResult,
                                  directory);
            return false;
        }
    }
    return true;
}

bool createTreeDirectories(const TextureTreeSnapshot& source,
                           TextureInstallResult& errorResult) {
    for (const auto& entry : source.entries) {
        if (entry.type != TextureTreeEntryType::Directory) {
            continue;
        }
        std::string destinationPath;
        if (!appendPath(paths::MinecraftCommon, entry.relativePath,
                        destinationPath)) {
            errorResult = failure(TextureInstallStage::Creating,
                                  TextureInstallError::DestinationCreate,
                                  ENAMETOOLONG, 0, paths::MinecraftCommon);
            return false;
        }
        const auto created = createDirectoryStrict(destinationPath.c_str());
        if (!created.succeeded) {
            errorResult = failure(TextureInstallStage::Creating,
                                  TextureInstallError::DestinationCreate,
                                  created.posixError, created.nativeResult,
                                  destinationPath);
            return false;
        }
    }
    return true;
}

bool copyFile(const std::string& sourcePath, const std::string& destinationPath,
              const std::uint64_t expectedSize,
              unsigned char* buffer, const std::size_t bufferSize,
              TextureFingerprintBuilder& fingerprint,
              const TextureInstallProgressCallback& progress,
              std::uint64_t& totalCopied,
              std::uint64_t& lastReportedPercent,
              TextureInstallResult& errorResult) noexcept {
    errno = 0;
    std::FILE* input = std::fopen(sourcePath.c_str(), "rb");
    if (input == nullptr) {
        errorResult = failure(TextureInstallStage::Copying,
                              TextureInstallError::SourceRead, errno,
                              lastNativeResult(), sourcePath);
        return false;
    }

    struct stat sourceInformation {};
    if (::fstat(::fileno(input), &sourceInformation) != 0 ||
        !S_ISREG(sourceInformation.st_mode) || sourceInformation.st_size < 0 ||
        static_cast<std::uint64_t>(sourceInformation.st_size) != expectedSize) {
        const int readError = errno != 0 ? errno : EIO;
        const auto nativeResult = lastNativeResult();
        std::fclose(input);
        errorResult = failure(TextureInstallStage::Copying,
                              TextureInstallError::SourceRead, readError,
                              nativeResult, sourcePath);
        return false;
    }

    errno = 0;
    std::FILE* output = std::fopen(destinationPath.c_str(), "wb");
    if (output == nullptr) {
        const int writeError = errno;
        const auto nativeResult = lastNativeResult();
        std::fclose(input);
        errorResult = failure(TextureInstallStage::Copying,
                              TextureInstallError::DestinationWrite,
                              writeError, nativeResult, destinationPath);
        return false;
    }

    std::uint64_t fileCopied = 0;
    bool succeeded = true;
    while (fileCopied < expectedSize) {
        const auto remaining = expectedSize - fileCopied;
        const auto request = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, bufferSize));
        errno = 0;
        const std::size_t bytesRead =
            std::fread(buffer, 1, request, input);
        if (bytesRead != request) {
            errorResult = failure(TextureInstallStage::Copying,
                                  TextureInstallError::SourceRead,
                                  errno != 0 ? errno : EIO,
                                  lastNativeResult(), sourcePath);
            succeeded = false;
            break;
        }
        // Copyに使う同じsource bytesをv1 fingerprintへ同時に流す。
        if (!fingerprint.updateFileBytes(buffer, bytesRead)) {
            errorResult = failure(TextureInstallStage::Copying,
                                  TextureInstallError::SourceRead, EIO, 0,
                                  sourcePath);
            succeeded = false;
            break;
        }

        std::size_t written = 0;
        while (written < bytesRead) {
            errno = 0;
            const std::size_t amount = std::fwrite(
                buffer + written, 1, bytesRead - written, output);
            if (amount == 0) {
                errorResult = failure(TextureInstallStage::Copying,
                                      TextureInstallError::DestinationWrite,
                                      errno != 0 ? errno : EIO,
                                      lastNativeResult(), destinationPath);
                succeeded = false;
                break;
            }
            written += amount;
        }
        if (!succeeded) {
            break;
        }

        fileCopied += static_cast<std::uint64_t>(bytesRead);
        totalCopied += static_cast<std::uint64_t>(bytesRead);
        const std::uint64_t percent =
            errorResult.totalBytes == 0
                ? 100
                : static_cast<std::uint64_t>(
                      (static_cast<long double>(totalCopied) * 100.0L) /
                      static_cast<long double>(errorResult.totalBytes));
        if (percent != lastReportedPercent) {
            lastReportedPercent = percent;
            emitProgress(progress, TextureInstallStage::Copying, totalCopied,
                         errorResult.totalBytes);
        }
    }

    if (succeeded && std::fflush(output) != 0) {
        errorResult = failure(TextureInstallStage::Copying,
                              TextureInstallError::DestinationWrite,
                              errno != 0 ? errno : EIO, lastNativeResult(),
                              destinationPath);
        succeeded = false;
    }
    if (std::fclose(output) != 0 && succeeded) {
        errorResult = failure(TextureInstallStage::Copying,
                              TextureInstallError::DestinationWrite,
                              errno != 0 ? errno : EIO, lastNativeResult(),
                              destinationPath);
        succeeded = false;
    }
    if (std::fclose(input) != 0 && succeeded) {
        errorResult = failure(TextureInstallStage::Copying,
                              TextureInstallError::SourceRead,
                              errno != 0 ? errno : EIO, lastNativeResult(),
                              sourcePath);
        succeeded = false;
    }
    return succeeded;
}

bool copySnapshot(const TexturePack& pack, const TextureTreeSnapshot& source,
                  unsigned char* buffer, const std::size_t bufferSize,
                  const TextureInstallProgressCallback& progress,
                  TextureInstallResult& result,
                  std::array<std::uint8_t, 32>& sourceFingerprint) {
    std::uint64_t totalCopied = 0;
    std::uint64_t lastReportedPercent = 0;
    std::uint64_t emptyFilesCopied = 0;
    const std::uint64_t progressTotal =
        source.totalBytes == 0
            ? std::max<std::uint64_t>(1, source.totalFiles)
            : source.totalBytes;
    TextureFingerprintBuilder fingerprint(source);
    emitProgress(progress, TextureInstallStage::Copying, 0, progressTotal);

    for (const auto& entry : source.entries) {
        if (!fingerprint.beginEntry(entry)) {
            result = failure(TextureInstallStage::Copying,
                             TextureInstallError::SourceRead, EIO, 0,
                             pack.commonPath);
            return false;
        }
        if (entry.type != TextureTreeEntryType::File) {
            continue;
        }
        std::string sourcePath;
        std::string destinationPath;
        if (!appendPath(pack.commonPath, entry.relativePath, sourcePath)) {
            result = failure(TextureInstallStage::Copying,
                             TextureInstallError::SourceRead, ENAMETOOLONG, 0,
                             pack.commonPath);
            return false;
        }
        if (!appendPath(paths::MinecraftCommon, entry.relativePath,
                        destinationPath)) {
            result = failure(TextureInstallStage::Copying,
                             TextureInstallError::DestinationWrite,
                             ENAMETOOLONG, 0, paths::MinecraftCommon);
            return false;
        }
        result.totalBytes = source.totalBytes;
        if (!copyFile(sourcePath, destinationPath, entry.size, buffer,
                      bufferSize, fingerprint, progress, totalCopied,
                      lastReportedPercent, result)) {
            return false;
        }
        if (source.totalBytes == 0) {
            ++emptyFilesCopied;
            const auto percent = static_cast<std::uint64_t>(
                (static_cast<long double>(emptyFilesCopied) * 100.0L) /
                static_cast<long double>(progressTotal));
            if (percent != lastReportedPercent) {
                lastReportedPercent = percent;
                emitProgress(progress, TextureInstallStage::Copying,
                             emptyFilesCopied, progressTotal);
            }
        }
    }

    if (!fingerprint.finish(sourceFingerprint)) {
        result = failure(TextureInstallStage::Copying,
                         TextureInstallError::SourceRead, EIO, 0,
                         pack.commonPath);
        return false;
    }
    emitProgress(progress, TextureInstallStage::Copying, progressTotal,
                 progressTotal);
    return true;
}

bool commitSdCard(TextureInstallResult& errorResult,
                  const TextureInstallStage stage) noexcept {
    const Result committed = fsdevCommitDevice("sdmc");
    if (R_SUCCEEDED(committed)) {
        return true;
    }
    errorResult = failure(stage, TextureInstallError::Commit, EIO,
                          static_cast<std::uint32_t>(committed),
                          paths::MinecraftCommon);
    return false;
}

bool cleanDestination(const TextureInstallProgressCallback& progress,
                      TextureInstallResult& result) noexcept {
    try {
        result.cleanupAttempted = true;
        emitProgress(progress, TextureInstallStage::CleaningUp, 0, 1);
        const auto partial =
            TextureTree::inspect(paths::MinecraftCommon, false);
        if (partial.state == TextureTreeState::Missing) {
            result.cleanupSucceeded = true;
            emitProgress(progress, TextureInstallStage::CleaningUp, 1, 1);
            return true;
        }
        if (partial.state != TextureTreeState::Ready) {
            result.cleanupSucceeded = false;
            return false;
        }

        TextureInstallResult cleanupError;
        result.cleanupSucceeded = removeSnapshot(
            paths::MinecraftCommon, partial, progress,
            TextureInstallStage::CleaningUp, cleanupError);
        if (result.cleanupSucceeded) {
            TextureInstallResult commitResult;
            result.cleanupSucceeded =
                commitSdCard(commitResult, TextureInstallStage::CleaningUp);
        }
        return result.cleanupSucceeded;
    } catch (...) {
        result.cleanupAttempted = true;
        result.cleanupSucceeded = false;
        return false;
    }
}

TextureInstallResult successfulResult(const TextureTreeSnapshot* source) {
    TextureInstallResult result;
    result.succeeded = true;
    result.error = TextureInstallError::None;
    result.stage = TextureInstallStage::Verifying;
    if (source != nullptr) {
        result.totalFiles = source->totalFiles;
        result.totalDirectories = source->totalDirectories;
        result.totalBytes = source->totalBytes;
    }
    return result;
}

} // namespace

TextureInstallResult TextureInstaller::apply(
    const TexturePack& pack,
    const TextureInstallProgressCallback& progress) noexcept {
    bool destinationTouched = false;
    try {
        emitProgress(progress, TextureInstallStage::Preflight, 0, 1);
        if (!isValidPack(pack)) {
            return failure(TextureInstallStage::Preflight,
                           TextureInstallError::InvalidSource, EINVAL, 0,
                           pack.commonPath);
        }

        const auto source = TextureTree::inspect(pack.commonPath, true);
        if (source.state != TextureTreeState::Ready) {
            return snapshotFailure(source, TextureInstallStage::Preflight,
                                   TextureInstallError::SourcePreflight);
        }
        const auto destination =
            TextureTree::inspect(paths::MinecraftCommon, false);
        if (destination.state == TextureTreeState::Error) {
            return snapshotFailure(destination, TextureInstallStage::Preflight,
                                   TextureInstallError::DestinationPreflight);
        }

        // destinationを変更する前にcopy用heap bufferを確保し、全fileで再利用する。
        std::unique_ptr<unsigned char[]> copyBuffer(
            new (std::nothrow) unsigned char[CopyBufferSize]);
        if (!copyBuffer) {
            return failure(TextureInstallStage::Preflight,
                           TextureInstallError::SourcePreflight, ENOMEM, 0,
                           pack.commonPath);
        }
        emitProgress(progress, TextureInstallStage::Preflight, 1, 1);

        TextureInstallResult result;
        result.totalFiles = source.totalFiles;
        result.totalDirectories = source.totalDirectories;
        result.totalBytes = source.totalBytes;

        if (destination.state == TextureTreeState::Ready) {
            destinationTouched = true;
            result.destinationTouched = true;
            TextureInstallResult removalError;
            if (!removeSnapshot(paths::MinecraftCommon, destination, progress,
                                TextureInstallStage::Removing,
                                removalError)) {
                removalError.destinationTouched = true;
                removalError.totalFiles = source.totalFiles;
                removalError.totalDirectories = source.totalDirectories;
                removalError.totalBytes = source.totalBytes;
                cleanDestination(progress, removalError);
                return removalError;
            }
        }

        destinationTouched = true;
        result.destinationTouched = true;
        emitProgress(progress, TextureInstallStage::Creating, 0, 1);
        if (!ensureDestinationRoot(result) ||
            !createTreeDirectories(source, result)) {
            cleanDestination(progress, result);
            return result;
        }
        emitProgress(progress, TextureInstallStage::Creating, 1, 1);

        std::array<std::uint8_t, 32> copiedSourceFingerprint{};
        if (!copySnapshot(pack, source, copyBuffer.get(), CopyBufferSize,
                          progress, result, copiedSourceFingerprint)) {
            cleanDestination(progress, result);
            return result;
        }
        copyBuffer.reset();

        emitProgress(progress, TextureInstallStage::Committing, 0, 1);
        if (!commitSdCard(result, TextureInstallStage::Committing)) {
            cleanDestination(progress, result);
            return result;
        }
        emitProgress(progress, TextureInstallStage::Committing, 1, 1);

        emitProgress(progress, TextureInstallStage::Verifying, 0, 1);
        // Preflight後にsourceを全文再読込せず、copy時に得たhashと照合する。
        if (source.fingerprint != copiedSourceFingerprint) {
            result = failure(TextureInstallStage::Verifying,
                             TextureInstallError::Verification, EIO, 0,
                             pack.commonPath);
            result.destinationTouched = true;
            result.totalFiles = source.totalFiles;
            result.totalDirectories = source.totalDirectories;
            result.totalBytes = source.totalBytes;
            cleanDestination(progress, result);
            return result;
        }

        const auto sourceAfter = TextureTree::inspect(pack.commonPath, false);
        if (sourceAfter.state != TextureTreeState::Ready ||
            !TextureTree::metadataEquivalent(source, sourceAfter)) {
            result = sourceAfter.state != TextureTreeState::Ready
                         ? snapshotFailure(sourceAfter,
                                           TextureInstallStage::Verifying,
                                           TextureInstallError::Verification)
                         : failure(TextureInstallStage::Verifying,
                                   TextureInstallError::Verification, EIO, 0,
                                   pack.commonPath);
            result.destinationTouched = true;
            result.totalFiles = source.totalFiles;
            result.totalDirectories = source.totalDirectories;
            result.totalBytes = source.totalBytes;
            cleanDestination(progress, result);
            return result;
        }

        auto destinationAfter =
            TextureTree::inspect(paths::MinecraftCommon, false);
        if (destinationAfter.state != TextureTreeState::Ready) {
            result = snapshotFailure(destinationAfter,
                                     TextureInstallStage::Verifying,
                                     TextureInstallError::Verification);
            result.destinationTouched = true;
            result.totalFiles = source.totalFiles;
            result.totalDirectories = source.totalDirectories;
            result.totalBytes = source.totalBytes;
            cleanDestination(progress, result);
            return result;
        }
        if (!TextureTree::metadataEquivalent(source, destinationAfter)) {
            result = failure(TextureInstallStage::Verifying,
                             TextureInstallError::Verification, EIO, 0,
                             paths::MinecraftCommon);
            result.destinationTouched = true;
            result.totalFiles = source.totalFiles;
            result.totalDirectories = source.totalDirectories;
            result.totalBytes = source.totalBytes;
            cleanDestination(progress, result);
            return result;
        }

        // DestinationはSDから必ずread-backし、書込み時hashだけを信用しない。
        destinationAfter = TextureTree::fingerprint(
            paths::MinecraftCommon, std::move(destinationAfter));
        if (destinationAfter.state != TextureTreeState::Ready ||
            !TextureTree::fingerprintsEqual(source, destinationAfter)) {
            result = destinationAfter.state != TextureTreeState::Ready
                         ? snapshotFailure(destinationAfter,
                                           TextureInstallStage::Verifying,
                                           TextureInstallError::Verification)
                         : failure(TextureInstallStage::Verifying,
                                   TextureInstallError::Verification, EIO, 0,
                                   paths::MinecraftCommon);
            result.destinationTouched = true;
            result.totalFiles = source.totalFiles;
            result.totalDirectories = source.totalDirectories;
            result.totalBytes = source.totalBytes;
            cleanDestination(progress, result);
            return result;
        }
        emitProgress(progress, TextureInstallStage::Verifying, 1, 1);
        auto succeeded = successfulResult(&source);
        succeeded.destinationTouched = true;
        return succeeded;
    } catch (...) {
        auto result = failure(
            destinationTouched ? TextureInstallStage::Copying
                               : TextureInstallStage::Preflight,
            destinationTouched ? TextureInstallError::DestinationWrite
                               : TextureInstallError::SourcePreflight,
            ENOMEM, 0,
            destinationTouched ? paths::MinecraftCommon : pack.commonPath);
        if (destinationTouched) {
            result.destinationTouched = true;
            cleanDestination(progress, result);
        }
        return result;
    }
}

TextureInstallResult TextureInstaller::restoreDefault(
    const TextureInstallProgressCallback& progress) noexcept {
    bool destinationTouched = false;
    try {
        emitProgress(progress, TextureInstallStage::Preflight, 0, 1);
        const auto destination =
            TextureTree::inspect(paths::MinecraftCommon, false);
        if (destination.state == TextureTreeState::Error) {
            return snapshotFailure(destination, TextureInstallStage::Preflight,
                                   TextureInstallError::DestinationPreflight);
        }
        emitProgress(progress, TextureInstallStage::Preflight, 1, 1);

        if (destination.state == TextureTreeState::Missing) {
            return successfulResult(nullptr);
        }

        TextureInstallResult result;
        destinationTouched = true;
        result.destinationTouched = true;
        if (!removeSnapshot(paths::MinecraftCommon, destination, progress,
                            TextureInstallStage::Removing, result)) {
            result.destinationTouched = true;
            cleanDestination(progress, result);
            return result;
        }

        emitProgress(progress, TextureInstallStage::Committing, 0, 1);
        if (!commitSdCard(result, TextureInstallStage::Committing)) {
            cleanDestination(progress, result);
            return result;
        }
        emitProgress(progress, TextureInstallStage::Committing, 1, 1);

        emitProgress(progress, TextureInstallStage::Verifying, 0, 1);
        const auto verified =
            TextureTree::inspect(paths::MinecraftCommon, false);
        if (verified.state != TextureTreeState::Missing) {
            result = verified.state == TextureTreeState::Error
                         ? snapshotFailure(verified,
                                           TextureInstallStage::Verifying,
                                           TextureInstallError::Verification)
                         : failure(TextureInstallStage::Verifying,
                                   TextureInstallError::Verification, EEXIST, 0,
                                   paths::MinecraftCommon);
            result.destinationTouched = true;
            cleanDestination(progress, result);
            return result;
        }
        emitProgress(progress, TextureInstallStage::Verifying, 1, 1);
        auto succeeded = successfulResult(nullptr);
        succeeded.destinationTouched = true;
        return succeeded;
    } catch (...) {
        auto result = failure(
            destinationTouched ? TextureInstallStage::Removing
                               : TextureInstallStage::Preflight,
            destinationTouched ? TextureInstallError::DestinationRemove
                               : TextureInstallError::DestinationPreflight,
            ENOMEM, 0, paths::MinecraftCommon);
        if (destinationTouched) {
            result.destinationTouched = true;
            cleanDestination(progress, result);
        }
        return result;
    }
}

} // namespace texnx::textures
