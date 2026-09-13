#include "texnx/textures/TextureTree.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using texnx::textures::TextureFingerprintBuilder;
using texnx::textures::TextureTree;
using texnx::textures::TextureTreeEntry;
using texnx::textures::TextureTreeEntryType;
using texnx::textures::TextureTreeSnapshot;
using texnx::textures::TextureTreeState;
using Contents =
    std::unordered_map<std::string, std::vector<std::uint8_t>>;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void require(const bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

TextureTreeSnapshot makeSnapshot(
    std::vector<TextureTreeEntry> entries) {
    TextureTreeSnapshot snapshot;
    snapshot.state = TextureTreeState::Ready;
    snapshot.entries = std::move(entries);
    for (const auto& entry : snapshot.entries) {
        if (entry.type == TextureTreeEntryType::Directory) {
            ++snapshot.totalDirectories;
        } else {
            ++snapshot.totalFiles;
            snapshot.totalBytes += entry.size;
        }
    }
    return snapshot;
}

void hashUint64(Sha256Context& context, const std::uint64_t value) {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(value >> (index * 8U));
    }
    sha256ContextUpdate(&context, bytes.data(), bytes.size());
}

std::array<std::uint8_t, 32> legacyFingerprint(
    const TextureTreeSnapshot& snapshot, const Contents& contents) {
    constexpr std::array<std::uint8_t, 18> header{
        'T', 'e', 'x', 'N', 'X', '-', 'C', 'o', 'm', 'm', 'o', 'n', '-', 'v',
        '1', 0, 0, 1};

    Sha256Context context{};
    sha256ContextCreate(&context);
    sha256ContextUpdate(&context, header.data(), header.size());
    hashUint64(context, static_cast<std::uint64_t>(snapshot.entries.size()));
    hashUint64(context, snapshot.totalFiles);
    hashUint64(context, snapshot.totalDirectories);
    hashUint64(context, snapshot.totalBytes);

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
            const auto found = contents.find(entry.relativePath);
            require(found != contents.end(), "fixture file is missing");
            require(found->second.size() == entry.size,
                    "fixture file size differs from metadata");
            sha256ContextUpdate(&context, found->second.data(),
                                found->second.size());
        }
    }

    std::array<std::uint8_t, 32> result{};
    sha256ContextGetHash(&context, result.data());
    return result;
}

std::array<std::uint8_t, 32> streamedFingerprint(
    const TextureTreeSnapshot& snapshot, const Contents& contents) {
    TextureFingerprintBuilder builder(snapshot);
    for (const auto& entry : snapshot.entries) {
        require(builder.beginEntry(entry), "entry framing failed");
        if (entry.type != TextureTreeEntryType::File) {
            continue;
        }
        const auto found = contents.find(entry.relativePath);
        require(found != contents.end(), "fixture file is missing");
        require(found->second.size() == entry.size,
                "fixture file size differs from metadata");

        const std::size_t split = found->second.size() / 2;
        require(builder.updateFileBytes(found->second.data(), split),
                "first streamed chunk failed");
        require(builder.updateFileBytes(found->second.data() + split,
                                        found->second.size() - split),
                "second streamed chunk failed");
    }

    std::array<std::uint8_t, 32> result{};
    require(builder.finish(result), "streamed fingerprint finish failed");
    return result;
}

struct DetectionResult {
    bool known{false};
    std::size_t matchedIndex{0};
    std::size_t fullHashes{0};
};

DetectionResult detectFixture(
    const TextureTreeSnapshot& installedMetadata,
    const Contents& installedContents,
    const std::vector<std::pair<TextureTreeSnapshot, Contents>>& packs) {
    DetectionResult result;
    auto installed = installedMetadata;
    bool installedHashed = false;

    for (std::size_t index = 0; index < packs.size(); ++index) {
        if (!TextureTree::metadataEquivalent(installedMetadata,
                                             packs[index].first)) {
            continue;
        }
        if (!installedHashed) {
            installed.fingerprint =
                streamedFingerprint(installedMetadata, installedContents);
            installedHashed = true;
            ++result.fullHashes;
        }
        auto candidate = packs[index].first;
        candidate.fingerprint =
            streamedFingerprint(candidate, packs[index].second);
        ++result.fullHashes;
        if (TextureTree::fingerprintsEqual(installed, candidate)) {
            result.known = true;
            result.matchedIndex = index;
            return result;
        }
    }
    return result;
}

} // namespace

int main() {
    try {
        const auto base = makeSnapshot({
            {TextureTreeEntryType::Directory, "empty", 0},
            {TextureTreeEntryType::Directory, "res", 0},
            {TextureTreeEntryType::File, "res/a.bin", 4},
            {TextureTreeEntryType::File, "res/b.bin", 3},
        });
        const Contents baseContents{
            {"res/a.bin", {1, 2, 3, 4}},
            {"res/b.bin", {5, 6, 7}},
        };

        // A: 同一treeはmetadataとv1 fingerprintが一致する。
        require(TextureTree::metadataEquivalent(base, base),
                "identical metadata did not match");
        const auto legacy = legacyFingerprint(base, baseContents);
        require(streamedFingerprint(base, baseContents) == legacy,
                "streamed fingerprint changed the v1 format");

        // B/C/E/F: path、type、size、extra file、empty directoryをmetadataで除外する。
        auto differentPath = base;
        differentPath.entries[2].relativePath = "res/c.bin";
        require(!TextureTree::metadataEquivalent(base, differentPath),
                "different path was not rejected");

        auto differentType = base;
        differentType.entries[0].type = TextureTreeEntryType::File;
        require(!TextureTree::metadataEquivalent(base, differentType),
                "different entry type was not rejected");

        auto differentSize = base;
        differentSize.entries[2].size = 5;
        ++differentSize.totalBytes;
        require(!TextureTree::metadataEquivalent(base, differentSize),
                "different size was not rejected");

        auto extraFile = base;
        extraFile.entries.push_back(
            {TextureTreeEntryType::File, "res/extra.bin", 1});
        ++extraFile.totalFiles;
        ++extraFile.totalBytes;
        require(!TextureTree::metadataEquivalent(base, extraFile),
                "extra file was not rejected");

        auto differentEmptyDirectory = base;
        differentEmptyDirectory.entries[0].relativePath = "other-empty";
        require(!TextureTree::metadataEquivalent(
                    base, differentEmptyDirectory),
                "empty directory difference was not rejected");

        // D: 同一path・sizeでも1 byte違えばFull SHA-256で不一致になる。
        auto changedContents = baseContents;
        changedContents["res/a.bin"][2] ^= 0x01U;
        require(TextureTree::metadataEquivalent(base, base),
                "same-size metadata unexpectedly differed");
        require(streamedFingerprint(base, changedContents) != legacy,
                "one-byte content change was not detected");

        // Metadata候補0件ならinstalledを含めFull hashは0回。
        const auto external = detectFixture(
            base, baseContents,
            {{differentPath, baseContents}, {extraFile, baseContents}});
        require(!external.known && external.fullHashes == 0,
                "metadata-only rejection performed a content hash");

        // 候補だけをsort順にhashし、最初の完全一致をKnownにする。
        const auto known = detectFixture(
            base, baseContents,
            {{differentPath, baseContents},
             {base, changedContents},
             {base, baseContents}});
        require(known.known && known.matchedIndex == 2 &&
                    known.fullHashes == 3,
                "candidate-only Known detection failed");

        const auto duplicate = detectFixture(
            base, baseContents,
            {{base, baseContents}, {base, baseContents}});
        require(duplicate.known && duplicate.matchedIndex == 0 &&
                    duplicate.fullHashes == 2,
                "first duplicate pack was not selected");

        // 手動の同一size 1-byte変更は候補になるがExternalとなる。
        const auto manuallyChanged = detectFixture(
            base, changedContents, {{base, baseContents}});
        require(!manuallyChanged.known && manuallyChanged.fullHashes == 2,
                "manual same-size modification was accepted");

        // G/H/I: preflight、copy-time、destination read-backの三者を照合する。
        auto preflight = base;
        preflight.fingerprint = streamedFingerprint(base, baseContents);
        auto copyTime = base;
        copyTime.fingerprint = streamedFingerprint(base, baseContents);
        auto destination = base;
        destination.fingerprint = streamedFingerprint(base, baseContents);
        require(TextureTree::fingerprintsEqual(preflight, copyTime) &&
                    TextureTree::fingerprintsEqual(preflight, destination),
                "valid apply fingerprints did not match");
        copyTime.fingerprint = streamedFingerprint(base, changedContents);
        require(!TextureTree::fingerprintsEqual(preflight, copyTime),
                "source change after preflight was not detected");
        require(!TextureTree::metadataEquivalent(preflight, extraFile),
                "source extra file after preflight was not detected");
        destination.fingerprint =
            streamedFingerprint(base, changedContents);
        require(!TextureTree::fingerprintsEqual(preflight, destination),
                "destination corruption was not detected");

        TextureFingerprintBuilder incomplete(base);
        require(incomplete.beginEntry(base.entries[0]),
                "directory framing failed");
        require(incomplete.beginEntry(base.entries[1]),
                "second directory framing failed");
        require(incomplete.beginEntry(base.entries[2]),
                "file framing failed");
        std::array<std::uint8_t, 32> incompleteResult{};
        require(!incomplete.finish(incompleteResult),
                "incomplete file stream was accepted");

        std::cout << "TEXTURE_PERFORMANCE_TESTS_OK\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "TEXTURE_PERFORMANCE_TESTS_FAILED: "
                  << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}
