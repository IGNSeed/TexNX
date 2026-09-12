#include "texnx/App.hpp"

#include "texnx/Paths.hpp"
#include "texnx/filesystem/FileSystem.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <switch.h>

namespace texnx {
namespace {

using filesystem::DirectoryCheckResult;
using filesystem::DirectoryState;

const char* statusText(const DirectoryState state) noexcept {
    switch (state) {
        case DirectoryState::Found:
            return "Found";
        case DirectoryState::NotFound:
            return "Not Found";
        case DirectoryState::Error:
            return "Error";
    }

    return "Error";
}

void printDirectoryStatus(const char* label, const char* path,
                          const DirectoryCheckResult& result) {
    std::printf("%s:\n%s\nStatus: %s\n", label, path, statusText(result.state));

    if (result.state == DirectoryState::Error) {
        const char* reason = std::strerror(result.posixError);
        std::printf("errno: %d (%s)\n", result.posixError,
                    reason != nullptr ? reason : "Unknown error");
        if (result.nativeResult != 0) {
            std::printf("libnx Result: 0x%08lX\n",
                        static_cast<unsigned long>(result.nativeResult));
        }
    }

    std::printf("\n");
}

} // namespace

int App::run() const {
    // 通常の NRO startup が libnx service と sdmc device を初期化する。
    consoleInit(nullptr);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad{};
    padInitializeDefault(&pad);

    const auto sdCard = filesystem::FileSystem::directoryExists(paths::SdCardRoot);
    const auto layeredFs =
        filesystem::FileSystem::directoryExists(paths::MinecraftLayeredFsRoot);
    const auto common = filesystem::FileSystem::directoryExists(paths::MinecraftCommon);

    std::printf("TexNX\n");
    std::printf("Version: %s\n\n", TEXNX_VERSION);
    std::printf("Minecraft: Nintendo Switch Edition\n");
    std::printf("Title ID: %s\n\n", paths::MinecraftTitleId);

    printDirectoryStatus("SD Card", paths::SdCardRoot, sdCard);
    printDirectoryStatus("LayeredFS", paths::MinecraftLayeredFsRoot, layeredFs);
    printDirectoryStatus("Common", paths::MinecraftCommon, common);

    std::printf("Press + to exit\n");
    consoleUpdate(nullptr);

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 buttonsDown = padGetButtonsDown(&pad);
        if ((buttonsDown & HidNpadButton_Plus) != 0) {
            break;
        }

        consoleUpdate(nullptr);
    }

    // Homebrew Menu へ戻る前に console resource を解放する。
    consoleExit(nullptr);
    return 0;
}

} // namespace texnx
