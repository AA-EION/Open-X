#include <juce_gui_basics/juce_gui_basics.h>
#include "UninstallerComponent.h"

namespace openx::uninstaller
{

class OpenXUninstallerApplication : public juce::JUCEApplication
{
public:
    OpenXUninstallerApplication() = default;

    const juce::String getApplicationName() override { return "Open-X Uninstaller"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront(true);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name,
                             openx::ui::OpenXLookAndFeel::BackgroundDark,
                             DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new UninstallerComponent(), true);

            setResizable(false, false);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplicationBase::quit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace openx::uninstaller

START_JUCE_APPLICATION(openx::uninstaller::OpenXUninstallerApplication)
