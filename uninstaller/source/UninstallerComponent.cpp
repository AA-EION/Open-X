#include "UninstallerComponent.h"

namespace openx::uninstaller
{

UninstallerComponent::UninstallerComponent()
    : progressBar(progressValue)
{
    setLookAndFeel(&lookAndFeel);

    // Title
    titleLabel.setText("OPEN-X DSP SUITE", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::AccentCyan);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Subtitle
    subtitleLabel.setText("Complete System Uninstaller", juce::dontSendNotification);
    subtitleLabel.setFont(juce::FontOptions(14.0f, juce::Font::plain));
    subtitleLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::TextMuted);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    // Description
    descriptionLabel.setText(
        "This tool completely and cleanly removes all Open-X plugins, application support "
        "files, preferences, system package receipts, and this uninstaller application.",
        juce::dontSendNotification
    );
    descriptionLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    descriptionLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::TextPrimary);
    descriptionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(descriptionLabel);

    // Details box
    detailsBox.setMultiLine(true);
    detailsBox.setReadOnly(true);
    detailsBox.setCaretVisible(false);
    detailsBox.setScrollbarsShown(true);
    detailsBox.setColour(juce::TextEditor::backgroundColourId, openx::ui::OpenXLookAndFeel::PanelDark);
    detailsBox.setColour(juce::TextEditor::outlineColourId, openx::ui::OpenXLookAndFeel::OutlineColour);
    detailsBox.setColour(juce::TextEditor::textColourId, openx::ui::OpenXLookAndFeel::TextPrimary);
    detailsBox.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    addAndMakeVisible(detailsBox);

    // Status Label
    statusLabel.setText("Ready to clean uninstall Open-X Suite.", juce::dontSendNotification);
    statusLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    statusLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::TextMuted);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    // Progress Bar (hidden by default)
    progressBar.setVisible(false);
    addAndMakeVisible(progressBar);

    // Uninstall Button
    uninstallButton.setButtonText("Uninstall Open-X Suite");
    uninstallButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8a1828));
    uninstallButton.setColour(juce::TextButton::buttonOnColourId, openx::ui::OpenXLookAndFeel::AccentRed);
    uninstallButton.setColour(juce::TextButton::textColourOffId, openx::ui::OpenXLookAndFeel::TextPrimary);
    uninstallButton.onClick = [this] { startUninstallation(); };
    addAndMakeVisible(uninstallButton);

    // Cancel Button
    cancelButton.setButtonText("Cancel");
    cancelButton.setColour(juce::TextButton::buttonColourId, openx::ui::OpenXLookAndFeel::PanelDark);
    cancelButton.setColour(juce::TextButton::textColourOffId, openx::ui::OpenXLookAndFeel::TextMuted);
    cancelButton.onClick = [] { juce::JUCEApplicationBase::quit(); };
    addAndMakeVisible(cancelButton);

    refreshComponentsList();
    setSize(520, 480);
}

UninstallerComponent::~UninstallerComponent()
{
    setLookAndFeel(nullptr);
}

void UninstallerComponent::refreshComponentsList()
{
    installedItems = UninstallerHelper::scanInstalledComponents();

    juce::String report;
    report << "Target Components to be Cleaned:\n";
    report << "--------------------------------------------------\n";

    int foundCount = 0;
    for (const auto& item : installedItems)
    {
        juce::String status = item.exists ? "[Installed] " : "[Not Found] ";
        if (item.exists)
            foundCount++;
        report << status.paddedRight(' ', 14) << item.name << " (" << item.category << ")\n";
        if (item.exists && item.path.isNotEmpty())
            report << "              " << item.path << "\n";
    }

    report << "\nSystem Cleanup Tasks:\n";
    report << "--------------------------------------------------\n";
    report << " [Pending]   System Package Receipts (pkgutil --forget)\n";
    report << " [Pending]   AudioComponentRegistrar Cache Reset\n";
    report << " [Pending]   Self-Removal of Open-X Uninstaller.app\n";

    detailsBox.setText(report);
}

void UninstallerComponent::startUninstallation()
{
    if (isWorking)
        return;

    isWorking = true;
    uninstallButton.setEnabled(false);
    cancelButton.setEnabled(false);
    statusLabel.setText("Preparing uninstallation...", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::AccentAmber);
    progressValue = -1.0;
    progressBar.setVisible(true);

    UninstallerHelper::promptAndExecuteUninstall(this, [this](bool success)
    {
        isWorking = false;
        progressBar.setVisible(false);
        uninstallButton.setEnabled(true);
        cancelButton.setEnabled(true);

        if (success)
        {
            statusLabel.setText("Uninstallation completed successfully.", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::AccentCyan);
            refreshComponentsList();
        }
        else
        {
            statusLabel.setText("Uninstallation was cancelled or failed.", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::AccentRed);
        }
    });
}

void UninstallerComponent::paint(juce::Graphics& g)
{
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Subtle header separator
    g.setColour(openx::ui::OpenXLookAndFeel::OutlineColour);
    g.drawHorizontalLine(90, 20.0f, (float)getWidth() - 20.0f);
    g.drawHorizontalLine(getHeight() - 75, 20.0f, (float)getWidth() - 20.0f);
}

void UninstallerComponent::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    titleLabel.setBounds(bounds.removeFromTop(30));
    subtitleLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(10); // Space

    descriptionLabel.setBounds(bounds.removeFromTop(38));
    bounds.removeFromTop(10);

    // Bottom buttons
    auto bottomArea = bounds.removeFromBottom(40);
    cancelButton.setBounds(bottomArea.removeFromRight(100));
    bottomArea.removeFromRight(15);
    uninstallButton.setBounds(bottomArea.removeFromRight(180));

    bounds.removeFromBottom(10);
    statusLabel.setBounds(bounds.removeFromBottom(22));
    progressBar.setBounds(bounds.removeFromBottom(8));
    bounds.removeFromBottom(10);

    // Remaining area for details box
    detailsBox.setBounds(bounds);
}

} // namespace openx::uninstaller
