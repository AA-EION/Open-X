#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

namespace openx::uninstaller
{

struct InstalledItem
{
    juce::String name;
    juce::String category; // "VST3 Plugin", "AudioUnit", "Application", "Data"
    juce::String path;
    bool exists = false;
};

class UninstallerHelper
{
public:
    /**
     * Checks whether the uninstaller is available on the current platform.
     */
    static bool isUninstallerAvailable();

    /**
     * Scans the system for installed Open-X components (VST3, AU, presets, app).
     */
    static std::vector<InstalledItem> scanInstalledComponents();

    /**
     * Shows a GUI confirmation modal and executes complete system uninstallation:
     * - Open-X VST3 and AU plugins (EQ-X, Comp-X, Limit-X, Verb-X, MB-X, DS-X, Gate-X)
     * - Application Support presets and cache
     * - System package receipts (pkgutil --forget)
     * - Resets AudioComponentRegistrar cache
     * - Cleanly uninstalls the Uninstaller itself
     */
    static void promptAndExecuteUninstall(juce::Component* parentComponent,
                                          std::function<void(bool success)> onComplete = nullptr);

    /**
     * Executes the actual uninstallation script and filesystem cleanup.
     */
    static bool executeUninstall();

private:
#if JUCE_MAC
    static bool executeMacOSUninstall();
#elif JUCE_WINDOWS
    static bool executeWindowsUninstall();
#endif
};

} // namespace openx::uninstaller
