#include "texnx/ui/TexturesView.hpp"

#include "texnx/Paths.hpp"
#include "texnx/localization/Localization.hpp"
#include "texnx/ui/Components.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/extern/nanovg/stb_image.h>

namespace texnx::ui {
namespace {

constexpr std::size_t MaximumIconFileBytes = 2U * 1024U * 1024U;
constexpr int MaximumIconDimension = 512;
constexpr int TextureIconUploadDimension = 80;
constexpr float TextureRowHeight = 112.0F;
constexpr float TextureIconFrameSize = 84.0F;
constexpr float TextureIconSize = 80.0F;
constexpr float TextureListInset = 8.0F;

struct TextureListEntry {
    std::string name;
    std::string description;
    std::string iconPath;
};

bool loadPng(brls::Image& image, const std::string& path) noexcept {
    try {
        struct stat information {};
        if (::stat(path.c_str(), &information) != 0 ||
            !S_ISREG(information.st_mode) || information.st_size <= 0 ||
            static_cast<std::uint64_t>(information.st_size) >
                MaximumIconFileBytes) {
            return false;
        }

        std::unique_ptr<std::FILE, decltype(&std::fclose)> input(
            std::fopen(path.c_str(), "rb"), &std::fclose);
        if (!input) {
            return false;
        }

        std::vector<unsigned char> encoded(
            static_cast<std::size_t>(information.st_size));
        if (std::fread(encoded.data(), 1, encoded.size(), input.get()) !=
            encoded.size()) {
            return false;
        }

        constexpr std::array<unsigned char, 8> PngSignature{
            0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
        if (encoded.size() < PngSignature.size() ||
            !std::equal(PngSignature.begin(), PngSignature.end(),
                        encoded.begin())) {
            return false;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        if (stbi_info_from_memory(encoded.data(),
                                  static_cast<int>(encoded.size()), &width,
                                  &height, &channels) == 0 ||
            width <= 0 || height <= 0 || width > MaximumIconDimension ||
            height > MaximumIconDimension ||
            static_cast<std::uint64_t>(width) *
                    static_cast<std::uint64_t>(height) >
                static_cast<std::uint64_t>(MaximumIconDimension) *
                    MaximumIconDimension) {
            return false;
        }

        int decodedWidth = 0;
        int decodedHeight = 0;
        int decodedChannels = 0;
        std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
            stbi_load_from_memory(encoded.data(),
                                  static_cast<int>(encoded.size()),
                                  &decodedWidth, &decodedHeight,
                                  &decodedChannels, 4),
            &stbi_image_free);
        if (!pixels || decodedWidth != width || decodedHeight != height) {
            return false;
        }

        int uploadWidth = width;
        int uploadHeight = height;
        const unsigned char* uploadPixels = pixels.get();
        std::vector<unsigned char> resizedPixels;

        const int largestDimension = std::max(width, height);
        if (largestDimension > TextureIconUploadDimension) {
            uploadWidth = std::max(
                1, width * TextureIconUploadDimension / largestDimension);
            uploadHeight = std::max(
                1, height * TextureIconUploadDimension / largestDimension);
            resizedPixels.resize(static_cast<std::size_t>(uploadWidth) *
                                 static_cast<std::size_t>(uploadHeight) * 4U);

            for (int destinationY = 0; destinationY < uploadHeight;
                 ++destinationY) {
                const int sourceY = destinationY * height / uploadHeight;
                for (int destinationX = 0; destinationX < uploadWidth;
                     ++destinationX) {
                    const int sourceX = destinationX * width / uploadWidth;
                    const std::size_t sourceOffset =
                        (static_cast<std::size_t>(sourceY) *
                             static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(sourceX)) *
                        4U;
                    const std::size_t destinationOffset =
                        (static_cast<std::size_t>(destinationY) *
                             static_cast<std::size_t>(uploadWidth) +
                         static_cast<std::size_t>(destinationX)) *
                        4U;
                    std::copy_n(pixels.get() + sourceOffset, 4,
                                resizedPixels.data() + destinationOffset);
                }
            }
            uploadPixels = resizedPixels.data();
        }

        const int texture = nvgCreateImageRGBA(
            brls::Application::getNVGContext(), uploadWidth, uploadHeight,
            NVG_IMAGE_NEAREST, uploadPixels);
        if (texture <= 0) {
            return false;
        }

        image.innerSetImage(texture);
        return true;
    } catch (...) {
        return false;
    }
}

class TextureCell final : public brls::Box {
public:
    TextureCell() : brls::Box(brls::Axis::ROW) {
        setAlignItems(brls::AlignItems::CENTER);
        setFocusable(true);
        setHeight(TextureRowHeight);
        setPadding(10, 16, 10, 16);
        setClipsToBounds(true);
        setHighlightCornerRadius(8);
        setLineBottom(1);
        setLineColor(
            brls::Application::getTheme()["brls/sidebar/separator"]);

        auto* iconFrame = new brls::Box(brls::Axis::ROW);
        iconFrame->setWidth(TextureIconFrameSize);
        iconFrame->setHeight(TextureIconFrameSize);
        iconFrame->setShrink(0);
        iconFrame->setAlignItems(brls::AlignItems::CENTER);
        iconFrame->setJustifyContent(brls::JustifyContent::CENTER);
        iconFrame->setMarginRight(20);
        iconFrame->setCornerRadius(8);
        iconFrame->setClipsToBounds(true);
        iconFrame->setBackgroundColor(
            brls::Application::getTheme()["texnx/background"]);

        icon_ = new brls::Image();
        icon_->setWidth(TextureIconSize);
        icon_->setHeight(TextureIconSize);
        icon_->setShrink(0);
        icon_->setCornerRadius(6);
        icon_->setClipsToBounds(true);
        icon_->setScalingType(brls::ImageScalingType::FIT);
        icon_->setFreeTexture(true);
        iconFrame->addView(icon_);
        addView(iconFrame);

        auto* textColumn = new brls::Box(brls::Axis::COLUMN);
        textColumn->setGrow(1);
        textColumn->setShrink(1);
        textColumn->setJustifyContent(brls::JustifyContent::CENTER);
        textColumn->setClipsToBounds(true);

        name_ = new brls::Label();
        name_->setWidthPercentage(100);
        name_->setHeight(38);
        name_->setFontSize(27);
        name_->setSingleLine(true);
        name_->setTextColor(brls::Application::getTheme()["texnx/text"]);
        textColumn->addView(name_);

        description_ = new brls::Label();
        description_->setWidthPercentage(100);
        description_->setHeight(30);
        description_->setFontSize(19);
        description_->setSingleLine(true);
        description_->setTextColor(
            brls::Application::getTheme()["texnx/text_secondary"]);
        textColumn->addView(description_);
        addView(textColumn);

        registerClickAction([this](brls::View*) {
            if (onClick_) {
                onClick_();
            }
            return true;
        });
        addGestureRecognizer(new brls::TapGestureRecognizer(this));

        inputTypeSubscription_ =
            brls::Application::getGlobalInputTypeChangeEvent()->subscribe(
                [this](const brls::InputType type) {
                    const bool focusedByController =
                        type == brls::InputType::GAMEPAD && focused;
                    setLineColor(
                        focusedByController
                            ? brls::TRANSPARENT
                            : brls::Application::getTheme()[
                                  "brls/sidebar/separator"]);
                });
    }

    ~TextureCell() override {
        brls::Application::getGlobalInputTypeChangeEvent()->unsubscribe(
            inputTypeSubscription_);
    }

    void configure(const TextureListEntry& entry,
                   std::function<void()> onClick) {
        name_->setText(entry.name);
        description_->setText(entry.description);
        description_->setVisibility(entry.description.empty()
                                        ? brls::Visibility::GONE
                                        : brls::Visibility::VISIBLE);
        onClick_ = std::move(onClick);

        if (!loadPng(*icon_, entry.iconPath) &&
            !loadPng(*icon_, paths::DefaultTextureIcon)) {
            brls::Logger::error("Unable to load built-in texture icon");
        }
    }

    brls::Label* nameLabel() const noexcept {
        return name_;
    }

    void onFocusGained() override {
        brls::Box::onFocusGained();
        if (brls::Application::getInputType() == brls::InputType::GAMEPAD) {
            setLineColor(brls::TRANSPARENT);
        }
    }

    void onFocusLost() override {
        brls::Box::onFocusLost();
        setLineColor(
            brls::Application::getTheme()["brls/sidebar/separator"]);
    }

private:
    brls::Image* icon_{nullptr};
    brls::Label* name_{nullptr};
    brls::Label* description_{nullptr};
    std::function<void()> onClick_;
    brls::Event<brls::InputType>::Subscription inputTypeSubscription_;
};

class TextureScrollingFrame final : public brls::ScrollingFrame {
public:
    TextureScrollingFrame() {
        setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    }

    void onChildFocusGained(brls::View* directChild,
                            brls::View* focusedView) override {
        // focus確定と同じ入力処理内で選択rowを完全表示位置へ移動する。
        brls::Box::onChildFocusGained(directChild, focusedView);
        childFocused = true;
        if (brls::Application::getInputType() == brls::InputType::GAMEPAD) {
            updateScrolling(false);
        }
    }
};

class BlockingProgressBox final : public brls::Box {
public:
    BlockingProgressBox() : brls::Box(brls::Axis::COLUMN) {
        setFocusable(true);
        setHideHighlight(true);
        registerClickAction([](brls::View*) { return true; });
        registerAction("", brls::BUTTON_B,
                       [](brls::View*) { return true; }, true, false);
        registerAction("", brls::BUTTON_START,
                       [](brls::View*) { return true; }, true, false);
    }
};

brls::Label* makeStateLabel(const std::string& text,
                            const brls::HorizontalAlign alignment) {
    auto* label = components::makeBody(text);
    label->setHeight(36);
    label->setFontSize(20);
    label->setHorizontalAlign(alignment);
    return label;
}

std::string replacePackPlaceholder(std::string text,
                                   const std::string& packName) {
    const std::string placeholder = "{pack}";
    std::size_t position = 0;
    while ((position = text.find(placeholder, position)) != std::string::npos) {
        text.replace(position, placeholder.size(), packName);
        position += packName.size();
    }
    return text;
}

std::string errorTextKey(const textures::TextureInstallResult& result) {
    using textures::TextureInstallError;
    if (result.error == TextureInstallError::SourcePreflight &&
        (result.posixError == ENOENT || result.posixError == ENOTDIR)) {
        return "textures.error_source_missing";
    }
    switch (result.error) {
        case TextureInstallError::InvalidSource:
        case TextureInstallError::SourcePreflight:
            return "textures.error_preflight";
        case TextureInstallError::DestinationPreflight:
            return "textures.error_destination_check";
        case TextureInstallError::DestinationRemove:
            return "textures.error_delete";
        case TextureInstallError::DestinationCreate:
            return "textures.error_create";
        case TextureInstallError::SourceRead:
            return "textures.error_source_read";
        case TextureInstallError::DestinationWrite:
            return "textures.error_copy";
        case TextureInstallError::Commit:
            return "textures.error_commit";
        case TextureInstallError::Verification:
            return "textures.error_verify";
        case TextureInstallError::Cleanup:
            return "textures.error_cleanup";
        case TextureInstallError::None:
            break;
    }
    return "textures.error_generic";
}

std::string progressTextKey(const textures::TextureInstallStage stage) {
    using textures::TextureInstallStage;
    switch (stage) {
        case TextureInstallStage::Preflight:
            return "textures.progress_checking";
        case TextureInstallStage::Removing:
            return "textures.progress_removing";
        case TextureInstallStage::Creating:
            return "textures.progress_creating";
        case TextureInstallStage::Copying:
            return "textures.progress_copying";
        case TextureInstallStage::Committing:
            return "textures.progress_saving";
        case TextureInstallStage::Verifying:
            return "textures.progress_verifying";
        case TextureInstallStage::CleaningUp:
            return "textures.progress_cleanup";
    }
    return "textures.progress_checking";
}

std::uint64_t progressPercent(
    const textures::TextureInstallProgress& progress) noexcept {
    if (progress.total == 0) {
        return 0;
    }
    if (progress.completed >= progress.total) {
        return 100;
    }
    return static_cast<std::uint64_t>(
        (static_cast<long double>(progress.completed) * 100.0L) /
        static_cast<long double>(progress.total));
}

} // namespace

struct TexturesView::AsyncBridge {
    TexturesView* view{nullptr};
};

TexturesView::TexturesView(localization::Localization& localization)
    : brls::Box(brls::Axis::COLUMN), localization_(localization),
      asyncBridge_(std::make_shared<AsyncBridge>()) {
    asyncBridge_->view = this;
    components::configureScreen(*this);
    components::registerBackAction(*this, localization_.text("common.back"));

    addView(components::makeTitle(localization_.text("textures.title")));

    panel_ = components::makePanel();
    panel_->setGrow(1);
    panel_->setClipsToBounds(true);

    currentLabel_ = makeStateLabel(
        localization_.text("textures.current") + ": " +
            localization_.text("textures.checking"),
        brls::HorizontalAlign::LEFT);
    panel_->addView(currentLabel_);

    statusLabel_ = makeStateLabel(localization_.text("textures.checking"),
                                  brls::HorizontalAlign::CENTER);
    panel_->addView(statusLabel_);

    listHost_ = new brls::Box(brls::Axis::COLUMN);
    listHost_->setGrow(1);
    listHost_->setShrink(1);
    listHost_->setMinHeight(0);
    listHost_->setWidthPercentage(100);
    listHost_->setClipsToBounds(true);
    panel_->addView(listHost_);
    addView(panel_);

    auto* backButton =
        components::makeBackButton(localization_.text("common.back"));
    addView(backButton);

    // fingerprint走査中もBackを即時操作できる。
    setDefaultFocusedIndex(2);
    startInitialLoad();
}

TexturesView::~TexturesView() {
    asyncBridge_->view = nullptr;
}

void TexturesView::startInitialLoad() {
    const auto bridge = asyncBridge_;
    brls::async([bridge] {
        auto scanResult = textures::TextureRepository::scan();
        auto currentResult = textures::TextureRepository::detectCurrentState(
            scanResult.state == textures::TextureScanState::Ready
                ? scanResult.packs
                : std::vector<textures::TexturePack>{});
        if (scanResult.state == textures::TextureScanState::Error &&
            currentResult.state ==
                textures::CurrentTextureState::ExternalOrUnknown) {
            currentResult.state = textures::CurrentTextureState::Error;
            currentResult.posixError = scanResult.posixError;
            currentResult.nativeResult = scanResult.nativeResult;
            currentResult.errorPath = paths::TextureDirectory;
        }
        brls::sync([bridge, scanResult = std::move(scanResult),
                    currentResult = std::move(currentResult)]() mutable {
            if (bridge->view != nullptr) {
                bridge->view->finishInitialLoad(std::move(scanResult),
                                                std::move(currentResult));
            }
        });
    });
}

void TexturesView::finishInitialLoad(
    textures::TextureScanResult scanResult,
    textures::CurrentTextureResult currentResult) {
    scanResult_ = std::move(scanResult);
    currentResult_ = std::move(currentResult);
    initialLoadFinished_ = true;

    if (scanResult_.state == textures::TextureScanState::Error) {
        brls::Logger::warning(
            "Unable to scan texture directory (errno {}, libnx Result {:#x})",
            scanResult_.posixError, scanResult_.nativeResult);
        statusLabel_->setText(localization_.text("textures.directory_error"));
        statusLabel_->setVisibility(brls::Visibility::VISIBLE);
    } else if (scanResult_.packs.empty()) {
        statusLabel_->setText(localization_.text("textures.empty"));
        statusLabel_->setVisibility(brls::Visibility::VISIBLE);
    } else {
        statusLabel_->setVisibility(brls::Visibility::GONE);
    }

    buildTextureList();
    updateCurrentUi();

    if (currentResult_.state == textures::CurrentTextureState::Error) {
        brls::Logger::error(
            "Current texture fingerprint failed (errno {}, libnx Result {:#x}, path '{}')",
            currentResult_.posixError, currentResult_.nativeResult,
            currentResult_.errorPath);
        auto* dialog =
            new brls::Dialog(localization_.text("textures.current_error"));
        dialog->addButton(localization_.text("common.ok"), [] {});
        dialog->open();
    }
}

void TexturesView::buildTextureList() {
    listHost_->clearViews();
    entryNameLabels_.clear();
    entryBaseNames_.clear();

    std::vector<TextureListEntry> entries;
    entries.reserve(scanResult_.packs.size() + 1U);
    entries.push_back({localization_.text("textures.default"),
                       localization_.text("textures.default_description"),
                       paths::DefaultTextureIcon});
    for (const auto& pack : scanResult_.packs) {
        entries.push_back({pack.name, pack.description, pack.iconPath});
    }

    auto* list = new TextureScrollingFrame();
    list->setGrow(1);
    list->setShrink(1);
    list->setMinHeight(0);
    list->setWidthPercentage(100);
    list->setClipsToBounds(true);

    auto* content = new brls::Box(brls::Axis::COLUMN);
    content->setPadding(TextureListInset);
    content->setDefaultFocusedIndex(0);

    brls::View* firstCell = nullptr;
    const auto bridge = asyncBridge_;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        auto* cell = new TextureCell();
        cell->setWidthPercentage(100);
        cell->setLineTop(index == 0 ? 1 : 0);
        cell->configure(entries[index], [bridge, index] {
            if (bridge->view != nullptr) {
                bridge->view->showConfirmation(index);
            }
        });
        content->addView(cell);
        if (firstCell == nullptr) {
            firstCell = cell;
        }
        entryNameLabels_.push_back(cell->nameLabel());
        entryBaseNames_.push_back(entries[index].name);
    }

    list->setContentView(content);
    listHost_->addView(list);
    listHost_->setDefaultFocusedIndex(0);
    panel_->setDefaultFocusedIndex(2);
    setDefaultFocusedIndex(1);
    if (firstCell != nullptr) {
        brls::Application::giveFocus(firstCell);
    }
}

void TexturesView::showConfirmation(const std::size_t entryIndex) {
    if (!initialLoadFinished_ || applying_ ||
        entryIndex > scanResult_.packs.size()) {
        return;
    }

    std::string question;
    std::string warning;
    std::string action;
    if (entryIndex == 0) {
        question = localization_.text("textures.restore_question");
        warning = localization_.text("textures.restore_warning");
        action = localization_.text("textures.restore");
    } else {
        question = replacePackPlaceholder(
            localization_.text("textures.apply_question"),
            scanResult_.packs[entryIndex - 1U].name);
        warning = localization_.text("textures.apply_warning");
        action = localization_.text("textures.apply");
    }

    auto* dialog = new brls::Dialog(question + "\n\n" + warning);
    dialog->addButton(localization_.text("common.cancel"), [] {});
    const auto bridge = asyncBridge_;
    dialog->addButton(action, [bridge, entryIndex] {
        if (bridge->view != nullptr) {
            bridge->view->beginOperation(entryIndex);
        }
    });
    dialog->open();
}

void TexturesView::beginOperation(const std::size_t entryIndex) {
    if (applying_ || entryIndex > scanResult_.packs.size()) {
        return;
    }
    applying_ = true;

    const bool restoreDefault = entryIndex == 0;
    const std::string operationName =
        restoreDefault ? localization_.text("textures.default")
                       : scanResult_.packs[entryIndex - 1U].name;

    auto* content = new BlockingProgressBox();
    content->setWidth(620);
    content->setPadding(38, 48, 38, 48);
    content->setAlignItems(brls::AlignItems::CENTER);

    auto* title = components::makeSectionLabel(
        replacePackPlaceholder(
            localization_.text(restoreDefault
                                   ? "textures.progress_restore_title"
                                   : "textures.progress_apply_title"),
            operationName));
    title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    content->addView(title);

    progressStageLabel_ = components::makeBody(
        localization_.text("textures.progress_checking"));
    progressStageLabel_->setHeight(42);
    progressStageLabel_->setMarginTop(14);
    content->addView(progressStageLabel_);

    progressPercentLabel_ = components::makeBody("0%");
    progressPercentLabel_->setHeight(46);
    progressPercentLabel_->setFontSize(28);
    content->addView(progressPercentLabel_);

    auto* progressTrack = new brls::Box(brls::Axis::ROW);
    progressTrack->setWidthPercentage(100);
    progressTrack->setHeight(14);
    progressTrack->setCornerRadius(7);
    progressTrack->setClipsToBounds(true);
    progressTrack->setBackgroundColor(
        brls::Application::getTheme()["brls/button/default_enabled_background"]);

    progressFill_ =
        new brls::Rectangle(brls::Application::getTheme()["texnx/text"]);
    progressFill_->setWidthPercentage(0);
    progressFill_->setHeight(14);
    progressTrack->addView(progressFill_);
    content->addView(progressTrack);

    progressDialog_ = new brls::Dialog(content);
    progressDialog_->setCancelable(false);
    progressDialog_->open();

    const auto bridge = asyncBridge_;
    const auto packs = scanResult_.packs;
    const textures::TexturePack selectedPack =
        restoreDefault ? textures::TexturePack{}
                       : scanResult_.packs[entryIndex - 1U];
    brls::async([bridge, packs, selectedPack, restoreDefault] {
        const auto progressCallback = [bridge](
                                          const textures::TextureInstallProgress&
                                              progress) {
            brls::sync([bridge, progress] {
                if (bridge->view != nullptr) {
                    bridge->view->updateProgress(progress);
                }
            });
        };

        auto result = restoreDefault
                          ? textures::TextureInstaller::restoreDefault(
                                progressCallback)
                          : textures::TextureInstaller::apply(
                                selectedPack, progressCallback);
        auto current =
            textures::TextureRepository::detectCurrentState(packs);
        brls::sync([bridge, result = std::move(result),
                    current = std::move(current)]() mutable {
            if (bridge->view != nullptr) {
                bridge->view->finishOperation(std::move(result),
                                              std::move(current));
            }
        });
    });
}

void TexturesView::updateProgress(
    const textures::TextureInstallProgress& progress) {
    if (!applying_ || progressStageLabel_ == nullptr ||
        progressPercentLabel_ == nullptr || progressFill_ == nullptr) {
        return;
    }

    const auto percent = std::min<std::uint64_t>(100, progressPercent(progress));
    progressStageLabel_->setText(
        localization_.text(progressTextKey(progress.stage)));
    progressPercentLabel_->setText(std::to_string(percent) + "%");
    progressFill_->setWidthPercentage(static_cast<float>(percent));
}

void TexturesView::finishOperation(
    textures::TextureInstallResult result,
    textures::CurrentTextureResult currentResult) {
    currentResult_ = std::move(currentResult);
    updateCurrentUi();

    if (!result.succeeded) {
        brls::Logger::error(
            "Texture operation failed (stage {}, error {}, errno {}, libnx Result {:#x}, path '{}', cleanup attempted {}, cleanup succeeded {})",
            static_cast<int>(result.stage), static_cast<int>(result.error),
            result.posixError, result.nativeResult, result.errorPath,
            result.cleanupAttempted, result.cleanupSucceeded);
    }

    auto* dialog = progressDialog_;
    progressDialog_ = nullptr;
    progressStageLabel_ = nullptr;
    progressPercentLabel_ = nullptr;
    progressFill_ = nullptr;

    const auto bridge = asyncBridge_;
    if (dialog == nullptr) {
        applying_ = false;
        showOperationResult(result, currentResult_);
        return;
    }
    dialog->close([bridge, result = std::move(result)] {
        if (bridge->view != nullptr) {
            bridge->view->applying_ = false;
            bridge->view->showOperationResult(result,
                                              bridge->view->currentResult_);
        }
    });
}

void TexturesView::showOperationResult(
    const textures::TextureInstallResult& result,
    const textures::CurrentTextureResult& current) {
    if (result.succeeded) {
        brls::Application::notify(
            localization_.text("textures.operation_complete"));
        if (current.state == textures::CurrentTextureState::Error) {
            auto* dialog =
                new brls::Dialog(localization_.text("textures.current_error"));
            dialog->addButton(localization_.text("common.ok"), [] {});
            dialog->open();
        }
        return;
    }

    std::string message = localization_.text("textures.operation_failed") +
                          "\n\n" +
                          localization_.text(errorTextKey(result));
    if (result.cleanupAttempted && !result.cleanupSucceeded) {
        message += "\n\n" + localization_.text("textures.cleanup_warning");
    }
    if (current.state == textures::CurrentTextureState::Error) {
        message += "\n\n" + localization_.text("textures.current_error");
    }
    auto* dialog = new brls::Dialog(message);
    dialog->addButton(localization_.text("common.ok"), [] {});
    dialog->open();
}

void TexturesView::updateCurrentUi() {
    std::string currentText;
    std::size_t selectedEntry = std::numeric_limits<std::size_t>::max();
    switch (currentResult_.state) {
        case textures::CurrentTextureState::Default:
            currentText = localization_.text("textures.default");
            selectedEntry = 0;
            break;
        case textures::CurrentTextureState::KnownPack:
            if (currentResult_.matchedPackIndex < scanResult_.packs.size()) {
                currentText =
                    scanResult_.packs[currentResult_.matchedPackIndex].name;
                selectedEntry = currentResult_.matchedPackIndex + 1U;
            } else {
                currentText = localization_.text("textures.current_error_short");
            }
            break;
        case textures::CurrentTextureState::ExternalOrUnknown:
            currentText = localization_.text("textures.external_unknown");
            break;
        case textures::CurrentTextureState::Error:
            currentText = localization_.text("textures.current_error_short");
            break;
    }

    currentLabel_->setText(localization_.text("textures.current") + ": " +
                           currentText);
    for (std::size_t index = 0; index < entryNameLabels_.size(); ++index) {
        entryNameLabels_[index]->setText(
            (index == selectedEntry ? "✓ " : "") + entryBaseNames_[index]);
    }
}

} // namespace texnx::ui
