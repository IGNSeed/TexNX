#include "texnx/textures/TextureTree.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace texnx::textures {
namespace {

constexpr std::array<std::uint8_t, 18> FingerprintHeader{
    'T', 'e', 'x', 'N', 'X', '-', 'C', 'o', 'm', 'm', 'o', 'n', '-', 'v', '1',
    0, 0, 1};

void hashUint64(Sha256Context& context, const std::uint64_t value) noexcept {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(value >> (index * 8U));
    }
    sha256ContextUpdate(&context, bytes.data(), bytes.size());
}

} // namespace

TextureFingerprintBuilder::TextureFingerprintBuilder(
    const TextureTreeSnapshot& snapshot) noexcept {
    if (snapshot.state != TextureTreeState::Ready) {
        return;
    }

    snapshot_ = &snapshot;
    expectedEntries_ = snapshot.entries.size();
    sha256ContextCreate(&context_);
    sha256ContextUpdate(&context_, FingerprintHeader.data(),
                        FingerprintHeader.size());
    hashUint64(context_, static_cast<std::uint64_t>(snapshot.entries.size()));
    hashUint64(context_, snapshot.totalFiles);
    hashUint64(context_, snapshot.totalDirectories);
    hashUint64(context_, snapshot.totalBytes);
    valid_ = true;
}

bool TextureFingerprintBuilder::beginEntry(
    const TextureTreeEntry& entry) noexcept {
    if (!valid_ || processedEntries_ >= expectedEntries_ ||
        (fileEntryOpen_ && processedFileBytes_ != expectedFileBytes_)) {
        valid_ = false;
        return false;
    }

    const auto& expectedEntry = snapshot_->entries[processedEntries_];
    if (entry.type != expectedEntry.type ||
        entry.relativePath != expectedEntry.relativePath ||
        entry.size != expectedEntry.size) {
        valid_ = false;
        return false;
    }

    const std::uint8_t type =
        entry.type == TextureTreeEntryType::Directory ? 'D' : 'F';
    sha256ContextUpdate(&context_, &type, sizeof(type));
    hashUint64(context_,
               static_cast<std::uint64_t>(entry.relativePath.size()));
    sha256ContextUpdate(&context_, entry.relativePath.data(),
                        entry.relativePath.size());
    hashUint64(context_, entry.size);

    ++processedEntries_;
    fileEntryOpen_ = entry.type == TextureTreeEntryType::File;
    expectedFileBytes_ = fileEntryOpen_ ? entry.size : 0;
    processedFileBytes_ = 0;
    return true;
}

bool TextureFingerprintBuilder::updateFileBytes(
    const void* data, const std::size_t size) noexcept {
    if (!valid_ || !fileEntryOpen_ || (size != 0 && data == nullptr) ||
        processedFileBytes_ > expectedFileBytes_ ||
        static_cast<std::uint64_t>(size) >
            expectedFileBytes_ - processedFileBytes_) {
        valid_ = false;
        return false;
    }

    if (size != 0) {
        sha256ContextUpdate(&context_, data, size);
        processedFileBytes_ += static_cast<std::uint64_t>(size);
    }
    return true;
}

bool TextureFingerprintBuilder::finish(
    std::array<std::uint8_t, 32>& fingerprint) noexcept {
    if (!valid_ || processedEntries_ != expectedEntries_ ||
        (fileEntryOpen_ && processedFileBytes_ != expectedFileBytes_)) {
        valid_ = false;
        return false;
    }

    sha256ContextGetHash(&context_, fingerprint.data());
    valid_ = false;
    return true;
}

bool TextureTree::metadataEquivalent(
    const TextureTreeSnapshot& left,
    const TextureTreeSnapshot& right) noexcept {
    if (left.state != right.state) {
        return false;
    }
    if (left.state != TextureTreeState::Ready) {
        return left.state == TextureTreeState::Missing;
    }
    if (left.totalFiles != right.totalFiles ||
        left.totalDirectories != right.totalDirectories ||
        left.totalBytes != right.totalBytes ||
        left.entries.size() != right.entries.size()) {
        return false;
    }

    return std::equal(
        left.entries.begin(), left.entries.end(), right.entries.begin(),
        [](const TextureTreeEntry& leftEntry,
           const TextureTreeEntry& rightEntry) {
            return leftEntry.type == rightEntry.type &&
                   leftEntry.relativePath == rightEntry.relativePath &&
                   leftEntry.size == rightEntry.size;
        });
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
