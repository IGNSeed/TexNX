#include "texnx/ui/TexturesView.hpp"

#include "texnx/Paths.hpp"
#include "texnx/localization/Localization.hpp"
#include "texnx/textures/TextureRepository.hpp"
#include "texnx/ui/Components.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/extern/nanovg/stb_image.h>

namespace texnx::ui {
namespace {

constexpr std::size_t MaximumIconFileBytes = 2 * 1024 * 1024;
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
                const int sourceY =
                    destinationY * height / uploadHeight;
                for (int destinationX = 0; destinationX < uploadWidth;
                     ++destinationX) {
                    const int sourceX =
                        destinationX * width / uploadWidth;
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
        name_->setTextColor(
            brls::Application::getTheme()["texnx/text"]);
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

        // Recycler外でもcontrollerとtouchで同じ暫定通知を実行する。
        registerClickAction([this](brls::View*) {
            brls::Application::notify(unavailableMessage_);
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
                   const std::string& unavailableMessage) {
        unavailableMessage_ = unavailableMessage;
        name_->setText(entry.name);
        description_->setText(entry.description);
        description_->setVisibility(entry.description.empty()
                                        ? brls::Visibility::GONE
                                        : brls::Visibility::VISIBLE);

        if (!loadPng(*icon_, entry.iconPath) &&
            !loadPng(*icon_, paths::DefaultTextureIcon)) {
            brls::Logger::error("Unable to load built-in texture icon");
        }
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
    std::string unavailableMessage_;
    brls::Event<brls::InputType>::Subscription inputTypeSubscription_;
};

class TextureScrollingFrame final : public brls::ScrollingFrame {
public:
    TextureScrollingFrame() {
        setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    }

    void onChildFocusGained(brls::View* directChild,
                            brls::View* focusedView) override {
        // focus borderがscroll animationより先にviewport外へ出ないよう、
        // focus確定と同じ入力処理内で選択rowを完全表示位置へ移動する。
        brls::Box::onChildFocusGained(directChild, focusedView);
        childFocused = true;
        if (brls::Application::getInputType() == brls::InputType::GAMEPAD) {
            updateScrolling(false);
        }
    }
};

std::vector<TextureListEntry> makeEntries(
    const textures::TextureScanResult& scanResult,
    const textures::CurrentTextureState currentState,
    localization::Localization& localization) {
    std::vector<TextureListEntry> entries;
    entries.reserve(scanResult.packs.size() + 1);

    const std::string defaultPrefix =
        currentState == textures::CurrentTextureState::Default ? "✓ " : "";
    entries.push_back({defaultPrefix + localization.text("textures.default"),
                       localization.text("textures.default_description"),
                       paths::DefaultTextureIcon});

    for (const auto& pack : scanResult.packs) {
        entries.push_back({pack.name, pack.description, pack.iconPath});
    }
    return entries;
}

brls::Label* makeStateLabel(const std::string& text,
                            const brls::HorizontalAlign alignment) {
    auto* label = components::makeBody(text);
    label->setHeight(36);
    label->setFontSize(20);
    label->setHorizontalAlign(alignment);
    return label;
}

} // namespace

TexturesView::TexturesView(localization::Localization& localization)
    : brls::Box(brls::Axis::COLUMN), localization_(localization) {
    components::configureScreen(*this);
    components::registerBackAction(*this, localization_.text("common.back"));

    addView(components::makeTitle(localization_.text("textures.title")));

    const auto scanResult = textures::TextureRepository::scan();
    const auto currentState =
        textures::TextureRepository::detectCurrentState();
    auto entries = makeEntries(scanResult, currentState, localization_);
    const bool hasEntries = !entries.empty();

    auto* panel = components::makePanel();
    panel->setGrow(1);
    panel->setClipsToBounds(true);

    const auto currentText =
        currentState == textures::CurrentTextureState::Default
            ? localization_.text("textures.default")
            : localization_.text("textures.external_unknown");
    panel->addView(makeStateLabel(
        localization_.text("textures.current") + ": " + currentText,
        brls::HorizontalAlign::LEFT));

    if (scanResult.state == textures::TextureScanState::Error) {
        brls::Logger::warning(
            "Unable to scan texture directory (errno {}, libnx Result {:#x})",
            scanResult.posixError, scanResult.nativeResult);
        panel->addView(makeStateLabel(
            localization_.text("textures.directory_error"),
            brls::HorizontalAlign::CENTER));
    } else if (scanResult.packs.empty()) {
        panel->addView(makeStateLabel(
            localization_.text("textures.empty"),
            brls::HorizontalAlign::CENTER));
    }

    if (hasEntries) {
        auto* list = new TextureScrollingFrame();
        list->setGrow(1);
        list->setShrink(1);
        list->setMinHeight(0);
        list->setWidthPercentage(100);
        list->setClipsToBounds(true);

        auto* content = new brls::Box(brls::Axis::COLUMN);
        content->setPadding(TextureListInset);
        content->setDefaultFocusedIndex(0);

        const auto unavailableMessage =
            localization_.text("textures.apply_unavailable");
        for (std::size_t index = 0; index < entries.size(); ++index) {
            auto* cell = new TextureCell();
            cell->setWidthPercentage(100);
            cell->setLineTop(index == 0 ? 1 : 0);
            cell->configure(entries[index], unavailableMessage);
            content->addView(cell);
        }

        list->setContentView(content);
        panel->addView(list);
        panel->setDefaultFocusedIndex(
            static_cast<int>(panel->getChildren().size() - 1));
    }

    addView(panel);
    auto* backButton =
        components::makeBackButton(localization_.text("common.back"));
    addView(backButton);

    // 通常はDefault entryがあるため一覧を選び、将来0件になった場合はBackを選ぶ。
    setDefaultFocusedIndex(hasEntries ? 1 : 2);
}

} // namespace texnx::ui
