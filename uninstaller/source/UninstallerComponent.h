#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "UninstallerHelper.h"

namespace openx::uninstaller
{

class UninstallerComponent : public juce::Component
{
public:
    UninstallerComponent();
    ~UninstallerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refreshComponentsList();
    void startUninstallation();

    openx::ui::OpenXLookAndFeel lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label descriptionLabel;

    juce::TextEditor detailsBox;
    juce::Label statusLabel;
    juce::ProgressBar progressBar;
    double progressValue = -1.0; // Indeterminate or 0..1

    juce::TextButton uninstallButton;
    juce::TextButton cancelButton;

    std::vector<InstalledItem> installedItems;
    bool isWorking = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UninstallerComponent)
};

} // namespace openx::uninstaller
