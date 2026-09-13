#include "texnx/ui/TexturesView.hpp"

#include "texnx/Paths.hpp"
#include "texnx/localization/Localization.hpp"
#include "texnx/ui/Components.hpp"
#include "texnx/ui/UiMetrics.hpp"

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
constexpr int TextureIconUploadDimension = 84;

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
        setHeight(metrics::TextureRowHeight);
        setPadding(8, 14, 8, 14);
        setClipsToBounds(true);
        setHighlightPadding(2);
        setHighlightCornerRadius(9);
        setLineBottom(1);
        setLineColor(brls::Application::getTheme()["texnx/divider"]);

        auto* iconFrame = new brls::Box(brls::Axis::ROW);
        iconFrame->setWidth(metrics::TextureIconFrameSize);
        iconFrame->setHeight(metrics::TextureIconFrameSize);
        iconFrame->setShrink(0);
        iconFrame->setAlignItems(brls::AlignItems::CENTER);
        iconFrame->setJustifyContent(brls::JustifyContent::CENTER);
        iconFrame->setMarginRight(16);
        iconFrame->setCornerRadius(8);
        iconFrame->setClipsToBounds(true);
        iconFrame->setBackgroundColor(
            brls::Application::getTheme()["texnx/icon_background"]);

        icon_ = new brls::Image();
        icon_->setWidth(metrics::TextureIconSize);
        icon_->setHeight(metrics::TextureIconSize);
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
        textColumn->setMinWidth(0);
        textColumn->setJustifyContent(brls::JustifyContent::CENTER);
        textColumn->setClipsToBounds(true);

        name_ = new brls::Label();
        name_->setWidthPercentage(100);
        name_->setHeight(34);
        name_->setFontSize(24);
        name_->setSingleLine(true);
        name_->setTextColor(brls::Application::getTheme()["texnx/text"]);
        textColumn->addView(name_);

        description_ = new brls::Label();
        description_->setWidthPercentage(100);
        description_->setHeight(26);
        description_->setFontSize(18);
        description_->setSingleLine(true);
        description_->setTextColor(
            brls::Application::getTheme()["texnx/text_secondary"]);
        textColumn->addView(description_);
        addView(textColumn);

        currentMarker_ = new brls::Label();
        currentMarker_->setWidth(30);
        currentMarker_->setHeight(40);
        currentMarker_->setShrink(0);
        currentMarker_->setMarginLeft(12);
        currentMarker_->setFontSize(19);
        currentMarker_->setSingleLine(true);
        currentMarker_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        currentMarker_->setTextColor(
            brls::Application::getTheme()["texnx/text"]);
        addView(currentMarker_);

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
                    setLineColor(
                        type == brls::InputType::GAMEPAD && focused
                            ? brls::TRANSPARENT
                            : brls::Application::getTheme()["texnx/divider"]);
                });
    }

    ~TextureCell() override {
        brls::Application::getGlobalInputTypeChangeEvent()->unsubscribe(
            inputTypeSubscription_);
    }

    void configure(const TextureListEntry& entry,
                   std::function<void()> onClick,
                   std::function<void()> onFocus) {
        name_->setText(entry.name);
        description_->setText(entry.description);
        description_->setVisibility(entry.description.empty()
                                        ? brls::Visibility::GONE
                                        : brls::Visibility::VISIBLE);
        onClick_ = std::move(onClick);
        onFocus_ = std::move(onFocus);

        if (!loadPng(*icon_, entry.iconPath) &&
            !loadPng(*icon_, paths::DefaultTextureIcon)) {
            brls::Logger::error("Unable to load built-in texture icon");
        }
    }

    brls::Label* currentMarker() const noexcept {
        return currentMarker_;
    }

    void onFocusGained() override {
        brls::Box::onFocusGained();
        if (onFocus_) {
            onFocus_();
        }
        if (brls::Application::getInputType() == brls::InputType::GAMEPAD) {
            setLineColor(brls::TRANSPARENT);
        }
    }

    void onFocusLost() override {
        brls::Box::onFocusLost();
        setLineColor(brls::Application::getTheme()["texnx/divider"]);
    }

private:
    brls::Image* icon_{nullptr};
    brls::Label* name_{nullptr};
    brls::Label* description_{nullptr};
    brls::Label* currentMarker_{nullptr};
    std::function<void()> onClick_;
    std::function<void()> onFocus_;
    brls::Event<brls::InputType>::Subscription inputTypeSubscription_;
};

class TextureScrollingFrame final : public brls::ScrollingFrame {
public:
    TextureScrollingFrame() {
        setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    }

    void onChildFocusGained(brls::View* directChild,
                            brls::View* focusedView) override {
        // focus確定と同じ入力処理内で選択行を完全表示位置へ移動する。
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
    components::configureContentPane(*this);

    title_ = components::makeTitle({});
    addView(title_);

    currentSectionLabel_ = components::makeSectionLabel({});
    currentSectionLabel_->setMarginTop(2);
    currentSectionLabel_->setMarginBottom(6);
    addView(currentSectionLabel_);

    auto* currentCard = new brls::Box(brls::Axis::ROW);
    currentCard->setWidthPercentage(100);
    currentCard->setHeight(metrics::CurrentCardHeight);
    currentCard->setShrink(0);
    currentCard->setPadding(metrics::CardPadding);
    currentCard->setAlignItems(brls::AlignItems::CENTER);
    currentCard->setCornerRadius(metrics::CardRadius);
    currentCard->setClipsToBounds(true);
    currentCard->setBackgroundColor(
        brls::Application::getTheme()["texnx/card"]);

    auto* iconFrame = new brls::Box(brls::Axis::ROW);
    iconFrame->setWidth(metrics::CurrentIconFrameSize);
    iconFrame->setHeight(metrics::CurrentIconFrameSize);
    iconFrame->setShrink(0);
    iconFrame->setAlignItems(brls::AlignItems::CENTER);
    iconFrame->setJustifyContent(brls::JustifyContent::CENTER);
    iconFrame->setMarginRight(20);
    iconFrame->setCornerRadius(10);
    iconFrame->setClipsToBounds(true);
    iconFrame->setBackgroundColor(
        brls::Application::getTheme()["texnx/icon_background"]);

    currentIcon_ = new brls::Image();
    currentIcon_->setWidth(metrics::CurrentIconSize);
    currentIcon_->setHeight(metrics::CurrentIconSize);
    currentIcon_->setShrink(0);
    currentIcon_->setCornerRadius(8);
    currentIcon_->setClipsToBounds(true);
    currentIcon_->setScalingType(brls::ImageScalingType::FIT);
    currentIcon_->setFreeTexture(true);
    iconFrame->addView(currentIcon_);
    currentCard->addView(iconFrame);

    auto* currentText = new brls::Box(brls::Axis::COLUMN);
    currentText->setGrow(1);
    currentText->setShrink(1);
    currentText->setMinWidth(0);
    currentText->setJustifyContent(brls::JustifyContent::CENTER);
    currentText->setClipsToBounds(true);

    currentBadge_ = new brls::Label();
    currentBadge_->setWidthPercentage(100);
    currentBadge_->setHeight(22);
    currentBadge_->setFontSize(15);
    currentBadge_->setSingleLine(true);
    currentBadge_->setTextColor(
        brls::Application::getTheme()["texnx/text_secondary"]);
    currentText->addView(currentBadge_);

    currentName_ = new brls::Label();
    currentName_->setWidthPercentage(100);
    currentName_->setHeight(36);
    currentName_->setFontSize(28);
    currentName_->setSingleLine(true);
    currentName_->setTextColor(
        brls::Application::getTheme()["texnx/text"]);
    currentText->addView(currentName_);

    currentDescription_ = new brls::Label();
    currentDescription_->setWidthPercentage(100);
    currentDescription_->setHeight(27);
    currentDescription_->setFontSize(18);
    currentDescription_->setSingleLine(true);
    currentDescription_->setTextColor(
        brls::Application::getTheme()["texnx/text_secondary"]);
    currentText->addView(currentDescription_);
    currentCard->addView(currentText);
    addView(currentCard);

    auto* divider = components::makeDivider();
    divider->setMarginTop(14);
    divider->setMarginBottom(10);
    addView(divider);

    availableLabel_ = components::makeSectionLabel({});
    availableLabel_->setMarginBottom(5);
    addView(availableLabel_);

    statusLabel_ = components::makeBody({});
    statusLabel_->setHeight(28);
    statusLabel_->setFontSize(17);
    statusLabel_->setMarginBottom(4);
    addView(statusLabel_);

    listHost_ = new brls::Box(brls::Axis::COLUMN);
    listHost_->setGrow(1);
    listHost_->setShrink(1);
    listHost_->setMinHeight(0);
    listHost_->setWidthPercentage(100);
    listHost_->setCornerRadius(metrics::CardRadius);
    listHost_->setClipsToBounds(true);
    listHost_->setBackgroundColor(
        brls::Application::getTheme()["texnx/card"]);
    addView(listHost_);

    registerAction({}, brls::BUTTON_B,
                   [this](brls::View*) {
                       if (!applying_ && returnToSidebar_) {
                           returnToSidebar_();
                       }
                       return true;
                   },
                   false, false, brls::SOUND_BACK);
    registerAction({}, brls::BUTTON_LEFT,
                   [this](brls::View*) {
                       if (!applying_ && returnToSidebar_) {
                           returnToSidebar_();
                       }
                       return true;
                   },
                   true, false, brls::SOUND_FOCUS_CHANGE);

    refreshText();
    showCheckingUi();
}

TexturesView::~TexturesView() {
    asyncBridge_->view = nullptr;
}

void TexturesView::onTabActivated() {
    active_ = true;
    requestRefresh();
}

void TexturesView::onTabDeactivated() {
    active_ = false;
}

void TexturesView::requestRefresh() {
    if (applying_) {
        refreshPending_ = true;
        return;
    }
    if (refreshInProgress_) {
        refreshPending_ = true;
        return;
    }

    refreshInProgress_ = true;
    dataReady_ = false;
    const std::size_t generation = ++refreshGeneration_;
    showCheckingUi();
    updateListActionAvailability();

    const auto bridge = asyncBridge_;
    brls::async([bridge, generation] {
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
        brls::sync([bridge, generation, scanResult = std::move(scanResult),
                    currentResult = std::move(currentResult)]() mutable {
            if (bridge->view != nullptr) {
                bridge->view->finishRefresh(generation, std::move(scanResult),
                                            std::move(currentResult));
            }
        });
    });
}

void TexturesView::finishRefresh(
    const std::size_t generation, textures::TextureScanResult scanResult,
    textures::CurrentTextureResult currentResult) {
    if (generation != refreshGeneration_) {
        return;
    }

    scanResult_ = std::move(scanResult);
    currentResult_ = std::move(currentResult);
    refreshInProgress_ = false;
    dataReady_ = true;

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

    if (active_ &&
        currentResult_.state == textures::CurrentTextureState::Error) {
        brls::Logger::error(
            "Current texture fingerprint failed (errno {}, libnx Result {:#x}, path '{}')",
            currentResult_.posixError, currentResult_.nativeResult,
            currentResult_.errorPath);
        auto* dialog =
            new brls::Dialog(localization_.text("textures.current_error"));
        dialog->addButton(localization_.text("common.ok"), [] {});
        dialog->open();
    }

    if (refreshPending_) {
        refreshPending_ = false;
        requestRefresh();
    }
}

void TexturesView::buildTextureList() {
    const auto* focused = brls::Application::getCurrentFocus();
    const bool hadListFocus =
        std::find(entryCells_.begin(), entryCells_.end(), focused) !=
        entryCells_.end();

    listHost_->clearViews();
    entryCells_.clear();
    entryCurrentMarkers_.clear();

    std::vector<TextureListEntry> entries;
    entries.reserve(scanResult_.packs.size() + 1U);
    entries.push_back({localization_.text("textures.default"),
                       localization_.text("textures.default_description"),
                       paths::DefaultTextureIcon});
    if (scanResult_.state == textures::TextureScanState::Ready) {
        for (const auto& pack : scanResult_.packs) {
            entries.push_back({pack.name, pack.description, pack.iconPath});
        }
    }

    auto* list = new TextureScrollingFrame();
    list->setGrow(1);
    list->setShrink(1);
    list->setMinHeight(0);
    list->setWidthPercentage(100);
    list->setClipsToBounds(true);

    auto* content = new brls::Box(brls::Axis::COLUMN);
    content->setPadding(metrics::TextureListInset);
    content->setDefaultFocusedIndex(0);

    const auto bridge = asyncBridge_;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        auto* cell = new TextureCell();
        cell->setWidthPercentage(100);
        cell->configure(
            entries[index],
            [bridge, index] {
                if (bridge->view != nullptr) {
                    bridge->view->showConfirmation(index);
                }
            },
            [bridge, index] {
                if (bridge->view != nullptr) {
                    bridge->view->lastFocusedIndex_ = index;
                }
            });
        cell->updateActionHint(brls::BUTTON_A,
                               localization_.text("hints.apply"));
        content->addView(cell);
        entryCells_.push_back(cell);
        entryCurrentMarkers_.push_back(cell->currentMarker());
    }

    for (std::size_t index = 0; index < entryCells_.size(); ++index) {
        entryCells_[index]->setCustomNavigationRoute(
            brls::FocusDirection::UP,
            entryCells_[index == 0 ? 0 : index - 1U]);
        entryCells_[index]->setCustomNavigationRoute(
            brls::FocusDirection::DOWN,
            entryCells_[index + 1U < entryCells_.size() ? index + 1U
                                                        : index]);
    }
    entryCells_.back()->setLineBottom(0);

    list->setContentView(content);
    listHost_->addView(list);
    listHost_->setDefaultFocusedIndex(0);
    updateListActionAvailability();

    if (hadListFocus && !entryCells_.empty()) {
        lastFocusedIndex_ =
            std::min(lastFocusedIndex_, entryCells_.size() - 1U);
        brls::Application::giveFocus(entryCells_[lastFocusedIndex_]);
    }
}

void TexturesView::showConfirmation(const std::size_t entryIndex) {
    if (!dataReady_ || applying_ ||
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
    if (applying_ || !dataReady_ ||
        entryIndex > scanResult_.packs.size()) {
        return;
    }
    setApplying(true);

    const bool restoreDefault = entryIndex == 0;
    const std::string operationName =
        restoreDefault ? localization_.text("textures.default")
                       : scanResult_.packs[entryIndex - 1U].name;

    auto* content = new BlockingProgressBox();
    content->setWidthPercentage(100);
    content->setHeight(metrics::LoadingDialogHeight);
    content->setPadding(metrics::LoadingPaddingVertical,
                        metrics::LoadingPaddingHorizontal,
                        metrics::LoadingPaddingVertical,
                        metrics::LoadingPaddingHorizontal);
    content->setAlignItems(brls::AlignItems::CENTER);
    content->setJustifyContent(brls::JustifyContent::CENTER);

    auto* contentColumn = new brls::Box(brls::Axis::COLUMN);
    contentColumn->setWidth(metrics::LoadingContentWidth);
    contentColumn->setAlignItems(brls::AlignItems::CENTER);

    auto* operationLabel = components::makeBody(localization_.text(
        restoreDefault ? "textures.progress_restore_operation"
                       : "textures.progress_apply_operation"));
    operationLabel->setHeight(metrics::LoadingOperationHeight);
    operationLabel->setFontSize(metrics::LoadingOperationSize);
    operationLabel->setSingleLine(true);
    operationLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    contentColumn->addView(operationLabel);

    auto* packNameLabel = components::makeSectionLabel(operationName);
    packNameLabel->setHeight(metrics::LoadingPackNameHeight);
    packNameLabel->setFontSize(metrics::LoadingPackNameSize);
    packNameLabel->setMarginTop(metrics::LoadingOperationToNameSpacing);
    packNameLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    packNameLabel->setAutoAnimate(false);
    packNameLabel->setAnimated(false);
    contentColumn->addView(packNameLabel);

    progressStageLabel_ = components::makeBody(
        localization_.text("textures.progress_checking"));
    progressStageLabel_->setHeight(metrics::LoadingStageHeight);
    progressStageLabel_->setMarginTop(metrics::LoadingNameToStageSpacing);
    progressStageLabel_->setSingleLine(true);
    progressStageLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    contentColumn->addView(progressStageLabel_);

    progressPercentLabel_ = components::makeBody("0%");
    progressPercentLabel_->setWidth(metrics::LoadingPercentWidth);
    progressPercentLabel_->setHeight(metrics::LoadingPercentHeight);
    progressPercentLabel_->setFontSize(metrics::LoadingPercentSize);
    progressPercentLabel_->setMarginTop(
        metrics::LoadingStageToPercentSpacing);
    progressPercentLabel_->setSingleLine(true);
    progressPercentLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    progressPercentLabel_->setTextColor(
        brls::Application::getTheme()["texnx/text"]);
    contentColumn->addView(progressPercentLabel_);

    auto* progressTrack = new brls::Box(brls::Axis::ROW);
    progressTrack->setWidthPercentage(
        metrics::LoadingProgressWidthPercent);
    progressTrack->setHeight(metrics::LoadingProgressHeight);
    progressTrack->setMarginTop(metrics::LoadingPercentToProgressSpacing);
    progressTrack->setCornerRadius(metrics::LoadingProgressHeight / 2.0F);
    progressTrack->setClipsToBounds(true);
    progressTrack->setBackgroundColor(
        brls::Application::getTheme()["texnx/progress_track"]);

    progressFill_ = new brls::Box(brls::Axis::ROW);
    progressFill_->setWidthPercentage(0);
    progressFill_->setHeight(metrics::LoadingProgressHeight);
    progressFill_->setCornerRadius(metrics::LoadingProgressHeight / 2.0F);
    progressFill_->setBackgroundColor(
        brls::Application::getTheme()["texnx/text"]);
    progressTrack->addView(progressFill_);
    contentColumn->addView(progressTrack);
    content->addView(contentColumn);

    progressDialog_ = new brls::Dialog(content);
    auto* dialogFrame = progressDialog_->getAppletFrame();
    dialogFrame->setWidth(metrics::LoadingDialogWidth);
    dialogFrame->setCornerRadius(metrics::CardRadius);
    dialogFrame->setClipsToBounds(true);
    dialogFrame->setBackgroundColor(
        brls::Application::getTheme()["texnx/card"]);
    progressDialog_->setCancelable(false);
    progressDialog_->open();

    const auto bridge = asyncBridge_;
    const auto packs = scanResult_.packs;
    const std::size_t selectedPackIndex =
        restoreDefault ? 0 : entryIndex - 1U;
    const textures::TexturePack selectedPack =
        restoreDefault ? textures::TexturePack{}
                       : scanResult_.packs[entryIndex - 1U];
    brls::async([bridge, packs, selectedPack, selectedPackIndex,
                 restoreDefault] {
        const auto progressCallback =
            [bridge](const textures::TextureInstallProgress& progress) {
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
        textures::CurrentTextureResult current;
        if (result.succeeded) {
            // Installerが検証した同一operationの結果を再利用し、再走査を避ける。
            current.state = restoreDefault
                                ? textures::CurrentTextureState::Default
                                : textures::CurrentTextureState::KnownPack;
            if (!restoreDefault) {
                current.matchedPackIndex = selectedPackIndex;
            }
        } else {
            current =
                textures::TextureRepository::detectCurrentState(packs);
        }
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
        setApplying(false);
        showOperationResult(result, currentResult_);
        return;
    }
    dialog->close([bridge, result = std::move(result)] {
        if (bridge->view != nullptr) {
            bridge->view->setApplying(false);
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

void TexturesView::showCheckingUi() {
    currentName_->setText(localization_.text("textures.checking"));
    currentDescription_->setText(
        localization_.text("textures.checking_description"));
    if (currentIconPath_ != paths::DefaultTextureIcon) {
        if (!loadPng(*currentIcon_, paths::DefaultTextureIcon)) {
            brls::Logger::error("Unable to load built-in current texture icon");
        }
        currentIconPath_ = paths::DefaultTextureIcon;
    }
    for (auto* marker : entryCurrentMarkers_) {
        marker->setText({});
    }
}

void TexturesView::updateCurrentUi() {
    std::string name;
    std::string description;
    std::string iconPath = paths::DefaultTextureIcon;
    std::size_t selectedEntry = std::numeric_limits<std::size_t>::max();

    switch (currentResult_.state) {
        case textures::CurrentTextureState::Default:
            name = localization_.text("textures.default");
            description =
                localization_.text("textures.default_description");
            selectedEntry = 0;
            break;
        case textures::CurrentTextureState::KnownPack:
            if (currentResult_.matchedPackIndex < scanResult_.packs.size()) {
                const auto& pack =
                    scanResult_.packs[currentResult_.matchedPackIndex];
                name = pack.name;
                description = pack.description;
                iconPath = pack.iconPath;
                selectedEntry = currentResult_.matchedPackIndex + 1U;
            } else {
                name = localization_.text("textures.unable_identify");
                description =
                    localization_.text("textures.current_error_description");
            }
            break;
        case textures::CurrentTextureState::ExternalOrUnknown:
            name = localization_.text("textures.external_unknown");
            description =
                localization_.text("textures.external_description");
            break;
        case textures::CurrentTextureState::Error:
            name = localization_.text("textures.unable_identify");
            description =
                localization_.text("textures.current_error_description");
            break;
    }

    currentName_->setText(name);
    currentDescription_->setText(description);
    if (currentIconPath_ != iconPath) {
        if (!loadPng(*currentIcon_, iconPath) &&
            !loadPng(*currentIcon_, paths::DefaultTextureIcon)) {
            brls::Logger::error("Unable to load current texture icon");
        }
        currentIconPath_ = iconPath;
    }

    for (std::size_t index = 0; index < entryCurrentMarkers_.size(); ++index) {
        entryCurrentMarkers_[index]->setText(index == selectedEntry ? "●" : "");
    }
}

void TexturesView::updateListActionAvailability() {
    const bool available = dataReady_ && !applying_;
    for (auto* cell : entryCells_) {
        cell->setActionAvailable(brls::BUTTON_A, available);
    }
}

void TexturesView::setApplying(const bool applying) {
    applying_ = applying;
    updateListActionAvailability();
    if (applyStateChanged_) {
        applyStateChanged_(applying);
    }
    if (!applying_ && refreshPending_ && !refreshInProgress_) {
        refreshPending_ = false;
        requestRefresh();
    }
}

void TexturesView::refreshText() {
    title_->setText(localization_.text("textures.title"));
    currentSectionLabel_->setText(
        localization_.text("textures.current_section"));
    currentBadge_->setText(localization_.text("textures.current_badge"));
    availableLabel_->setText(
        localization_.text("textures.available_section"));
    updateActionHint(brls::BUTTON_B, localization_.text("hints.tabs"));

    if (!dataReady_) {
        statusLabel_->setText(localization_.text("textures.checking"));
        statusLabel_->setVisibility(brls::Visibility::VISIBLE);
    } else if (scanResult_.state == textures::TextureScanState::Error) {
        statusLabel_->setText(localization_.text("textures.directory_error"));
        statusLabel_->setVisibility(brls::Visibility::VISIBLE);
    } else if (scanResult_.packs.empty()) {
        statusLabel_->setText(localization_.text("textures.empty"));
        statusLabel_->setVisibility(brls::Visibility::VISIBLE);
    } else {
        statusLabel_->setVisibility(brls::Visibility::GONE);
    }

    buildTextureList();
    if (dataReady_) {
        updateCurrentUi();
    } else {
        showCheckingUi();
    }
}

void TexturesView::focusContent() {
    if (entryCells_.empty()) {
        return;
    }
    lastFocusedIndex_ =
        std::min(lastFocusedIndex_, entryCells_.size() - 1U);
    brls::Application::giveFocus(entryCells_[lastFocusedIndex_]);
}

void TexturesView::setReturnToSidebarCallback(
    std::function<void()> callback) {
    returnToSidebar_ = std::move(callback);
}

void TexturesView::setApplyStateCallback(
    std::function<void(bool)> callback) {
    applyStateChanged_ = std::move(callback);
}

} // namespace texnx::ui
