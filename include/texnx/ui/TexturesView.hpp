#pragma once

#include "texnx/textures/TextureInstaller.hpp"
#include "texnx/textures/TextureRepository.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <borealis.hpp>

namespace texnx::localization {
class Localization;
}

namespace texnx::ui {

class TexturesView final : public brls::Box {
public:
    explicit TexturesView(localization::Localization& localization);
    ~TexturesView() override;

    void onTabActivated();
    void onTabDeactivated();
    void refreshText();
    void focusContent();
    void setReturnToSidebarCallback(std::function<void()> callback);
    void setApplyStateCallback(std::function<void(bool)> callback);

private:
    struct AsyncBridge;

    void requestRefresh();
    void finishRefresh(std::size_t generation,
                       textures::TextureScanResult scanResult,
                       textures::CurrentTextureResult currentResult);
    void buildTextureList();
    void showConfirmation(std::size_t entryIndex);
    void beginOperation(std::size_t entryIndex);
    void updateProgress(const textures::TextureInstallProgress& progress);
    void finishOperation(textures::TextureInstallResult result,
                         textures::CurrentTextureResult currentResult);
    void showOperationResult(const textures::TextureInstallResult& result,
                             const textures::CurrentTextureResult& current);
    void showCheckingUi();
    void updateCurrentUi();
    void updateListActionAvailability();
    void setApplying(bool applying);

    localization::Localization& localization_;
    std::shared_ptr<AsyncBridge> asyncBridge_;
    textures::TextureScanResult scanResult_{};
    textures::CurrentTextureResult currentResult_{};
    brls::Box* listHost_{nullptr};
    brls::Label* title_{nullptr};
    brls::Label* currentSectionLabel_{nullptr};
    brls::Image* currentIcon_{nullptr};
    brls::Label* currentBadge_{nullptr};
    brls::Label* currentName_{nullptr};
    brls::Label* currentDescription_{nullptr};
    brls::Label* availableLabel_{nullptr};
    brls::Label* statusLabel_{nullptr};
    brls::Dialog* progressDialog_{nullptr};
    brls::Label* progressStageLabel_{nullptr};
    brls::Label* progressPercentLabel_{nullptr};
    brls::Box* progressFill_{nullptr};
    std::vector<brls::View*> entryCells_;
    std::vector<brls::Label*> entryCurrentMarkers_;
    std::string currentIconPath_;
    std::function<void()> returnToSidebar_;
    std::function<void(bool)> applyStateChanged_;
    std::size_t lastFocusedIndex_{0};
    std::size_t refreshGeneration_{0};
    bool dataReady_{false};
    bool refreshInProgress_{false};
    bool refreshPending_{false};
    bool active_{false};
    bool applying_{false};
};

} // namespace texnx::ui
