#pragma once

#include "texnx/textures/TextureInstaller.hpp"
#include "texnx/textures/TextureRepository.hpp"

#include <cstddef>
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

private:
    struct AsyncBridge;

    void startInitialLoad();
    void finishInitialLoad(textures::TextureScanResult scanResult,
                           textures::CurrentTextureResult currentResult);
    void buildTextureList();
    void showConfirmation(std::size_t entryIndex);
    void beginOperation(std::size_t entryIndex);
    void updateProgress(const textures::TextureInstallProgress& progress);
    void finishOperation(textures::TextureInstallResult result,
                         textures::CurrentTextureResult currentResult);
    void showOperationResult(const textures::TextureInstallResult& result,
                             const textures::CurrentTextureResult& current);
    void updateCurrentUi();

    localization::Localization& localization_;
    std::shared_ptr<AsyncBridge> asyncBridge_;
    textures::TextureScanResult scanResult_{};
    textures::CurrentTextureResult currentResult_{};
    brls::Box* panel_{nullptr};
    brls::Box* listHost_{nullptr};
    brls::Label* currentLabel_{nullptr};
    brls::Label* statusLabel_{nullptr};
    brls::Dialog* progressDialog_{nullptr};
    brls::Label* progressStageLabel_{nullptr};
    brls::Label* progressPercentLabel_{nullptr};
    brls::Rectangle* progressFill_{nullptr};
    std::vector<brls::Label*> entryNameLabels_;
    std::vector<std::string> entryBaseNames_;
    bool initialLoadFinished_{false};
    bool applying_{false};
};

} // namespace texnx::ui
