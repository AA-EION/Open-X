#include "UninstallerHelper.h"

#if !JUCE_MAC

namespace openx::uninstaller
{

static const std::vector<juce::String> kPluginNames = {
    "EQ-X",
    "Comp-X",
    "Limit-X",
    "Verb-X",
    "MB-X",
    "DS-X",
    "Gate-X"
};

bool UninstallerHelper::isUninstallerAvailable()
{
#if JUCE_WINDOWS
    return true;
#else
    return false;
#endif
}

std::vector<InstalledItem> UninstallerHelper::scanInstalledComponents()
{
    std::vector<InstalledItem> items;
#if JUCE_WINDOWS
    juce::File commonFiles = juce::File::getSpecialLocation(juce::File::commonFilesDirectory);
    juce::File vst3Dir = commonFiles.getChildFile("VST3");

    for (const auto& name : kPluginNames)
    {
        juce::File pluginDir = vst3Dir.getChildFile(name + ".vst3");
        items.push_back({ name + " (VST3)", "VST3 Plugin", pluginDir.getFullPathName(), pluginDir.exists() });
    }
#endif
    return items;
}

bool UninstallerHelper::executeUninstall()
{
#if JUCE_WINDOWS
    return executeWindowsUninstall();
#else
    return false;
#endif
}

#if JUCE_WINDOWS
bool UninstallerHelper::executeWindowsUninstall()
{
    // On Windows, the MSI installer registers uninstallation with Windows Installer.
    // We launch Windows "msiexec /x" for the Open-X Suite or open Apps & Features.
    juce::ChildProcess process;
    juce::StringArray args;
    args.add("powershell.exe");
    args.add("-Command");
    args.add("Start-Process ms-settings:appsfeatures");
    return process.start(args);
}
#endif

void UninstallerHelper::promptAndExecuteUninstall(juce::Component* parentComponent,
                                                  std::function<void(bool success)> onComplete)
{
    juce::String title = "Uninstall Open-X DSP Suite";
    juce::String message =
        "On Windows, Open-X DSP Suite can be completely uninstalled from Windows Settings -> Installed Apps (or Control Panel -> Programs and Features).\n\n"
        "Would you like to open Installed Apps now?";

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle(title)
        .withMessage(message)
        .withButton("Open Settings")
        .withButton("Cancel")
        .withParentComponent(parentComponent);

    juce::AlertWindow::showAsync(options, [onComplete](int result)
    {
        if (result == 1)
        {
            bool ok = executeUninstall();
            if (onComplete)
                onComplete(ok);
        }
        else if (onComplete)
        {
            onComplete(false);
        }
    });
}

} // namespace openx::uninstaller
#endif
