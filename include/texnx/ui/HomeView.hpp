#pragma once

#include <functional>

#include <borealis.hpp>

namespace texnx::localization {
class Localization;
}

namespace texnx::ui {

enum class Screen {
    Textures,
    Settings,
    About,
};

class HomeView final : public brls::Box {
public:
    using OpenScreenCallback = std::function<void(Screen)>;

    HomeView(localization::Localization& localization, OpenScreenCallback openScreen);
    void refreshText();

private:
    localization::Localization& localization_;
    brls::Label* tagline_{nullptr};
    brls::Button* texturesButton_{nullptr};
    brls::Button* settingsButton_{nullptr};
    brls::Button* aboutButton_{nullptr};
};

} // namespace texnx::ui
