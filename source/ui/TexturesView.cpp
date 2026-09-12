#include "texnx/ui/TexturesView.hpp"

#include "texnx/Paths.hpp"
#include "texnx/localization/Localization.hpp"
#include "texnx/textures/TextureRepository.hpp"
#include "texnx/ui/Components.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include <borealis/extern/nanovg/stb_image.h>

namespace texnx::ui {
namespace {

constexpr char TextureCellIdentifier[] = "TexNXTextureCell";
constexpr std::size_t MaximumIconFileBytes = 2 * 1024 * 1024;
constexpr int MaximumIconDimension = 512;
constexpr float TextureRowHeight = 112.0F;
constexpr float TextureIconFrameSize = 84.0F;
constexpr float TextureIconSize = 80.0F;

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

        const int texture = nvgCreateImageRGBA(
            brls::Application::getNVGContext(), width, height,
            NVG_IMAGE_NEAREST, pixels.get());
        if (texture <= 0) {
            return false;
        }

        image.innerSetImage(texture);
        return true;
    } catch (...) {
        return false;
    }
}

class TextureCell final : public brls::RecyclerCell {
public:
    TextureCell() {
        setAxis(brls::Axis::ROW);
        setAlignItems(brls::AlignItems::CENTER);
        setFocusable(true);
        setHeight(TextureRowHeight);
        setPadding(10, 16, 10, 16);
        setClipsToBounds(true);
        setHighlightCornerRadius(8);

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
    }

    void prepareForReuse() override {
        icon_->clear();
        name_->setText("");
        description_->setText("");
        description_->setVisibility(brls::Visibility::GONE);
    }

    void configure(const TextureListEntry& entry) {
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

private:
    brls::Image* icon_{nullptr};
    brls::Label* name_{nullptr};
    brls::Label* description_{nullptr};
};

class TextureDataSource final : public brls::RecyclerDataSource {
public:
    TextureDataSource(std::vector<TextureListEntry> entries,
                      std::string unavailableMessage)
        : entries_(std::move(entries)),
          unavailableMessage_(std::move(unavailableMessage)) {}

    int numberOfRows(brls::RecyclerFrame*, int) override {
        const auto maximum =
            static_cast<std::size_t>(std::numeric_limits<int>::max());
        return static_cast<int>(std::min(entries_.size(), maximum));
    }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler,
                                   const brls::IndexPath index) override {
        auto* cell = static_cast<TextureCell*>(
            recycler->dequeueReusableCell(TextureCellIdentifier));
        cell->configure(entries_[static_cast<std::size_t>(index.row)]);
        return cell;
    }

    float heightForRow(brls::RecyclerFrame*, brls::IndexPath) override {
        return TextureRowHeight;
    }

    void didSelectRowAt(brls::RecyclerFrame*,
                        const brls::IndexPath index) override {
        if (index.row >= 0 &&
            static_cast<std::size_t>(index.row) < entries_.size()) {
            brls::Application::notify(unavailableMessage_);
        }
    }

private:
    std::vector<TextureListEntry> entries_;
    std::string unavailableMessage_;
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
        auto* recycler = new brls::RecyclerFrame();
        recycler->setGrow(1);
        recycler->setWidthPercentage(100);
        recycler->setClipsToBounds(true);
        recycler->estimatedRowHeight = TextureRowHeight;
        recycler->registerCell(TextureCellIdentifier,
                               [] { return new TextureCell(); });
        recycler->setDefaultCellFocus(brls::IndexPath(0, 0));
        recycler->setDataSource(new TextureDataSource(
            std::move(entries),
            localization_.text("textures.apply_unavailable")));
        panel->addView(recycler);
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
