#include "UninstallerHelper.h"

#if JUCE_MAC
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <cstdlib>

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
    return true;
}

std::vector<InstalledItem> UninstallerHelper::scanInstalledComponents()
{
    std::vector<InstalledItem> items;
    juce::File userHome = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    // 1. VST3 plugins (System and User)
    for (const auto& name : kPluginNames)
    {
        juce::File sysVst3 = juce::File("/Library/Audio/Plug-Ins/VST3").getChildFile(name + ".vst3");
        juce::File userVst3 = userHome.getChildFile("Library/Audio/Plug-Ins/VST3/" + name + ".vst3");

        bool exists = sysVst3.exists() || userVst3.exists();
        juce::String path = sysVst3.exists() ? sysVst3.getFullPathName() : userVst3.getFullPathName();
        items.push_back({ name + " (VST3)", "VST3 Plugin", path, exists });
    }

    // 2. AU plugins (System and User)
    for (const auto& name : kPluginNames)
    {
        juce::File sysAu = juce::File("/Library/Audio/Plug-Ins/Components").getChildFile(name + ".component");
        juce::File userAu = userHome.getChildFile("Library/Audio/Plug-Ins/Components/" + name + ".component");

        bool exists = sysAu.exists() || userAu.exists();
        juce::String path = sysAu.exists() ? sysAu.getFullPathName() : userAu.getFullPathName();
        items.push_back({ name + " (AU)", "AudioUnit", path, exists });
    }

    // 3. Application Support / Presets
    {
        juce::File sysData("/Library/Application Support/Open-X");
        juce::File userData = userHome.getChildFile("Library/Application Support/Open-X");
        bool exists = sysData.exists() || userData.exists();
        items.push_back({ "Presets & Application Support", "Data", userData.getFullPathName(), exists });
    }

    // 4. Uninstaller App
    {
        juce::File appPath("/Applications/Open-X Uninstaller.app");
        juce::File currentApp = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        bool exists = appPath.exists() || currentApp.exists();
        items.push_back({ "Open-X Uninstaller", "Application", appPath.getFullPathName(), exists });
    }

    return items;
}

bool UninstallerHelper::executeUninstall()
{
    return executeMacOSUninstall();
}

bool UninstallerHelper::executeMacOSUninstall()
{
    juce::File userHome = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    juce::File currentApp = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    juce::String currentAppPath = currentApp.getFullPathName();

    // 1. Delete user-level files directly without requiring administrator rights
    for (const auto& name : kPluginNames)
    {
        userHome.getChildFile("Library/Audio/Plug-Ins/VST3/" + name + ".vst3").deleteRecursively();
        userHome.getChildFile("Library/Audio/Plug-Ins/Components/" + name + ".component").deleteRecursively();
    }
    userHome.getChildFile("Library/Application Support/Open-X").deleteRecursively();
    userHome.getChildFile("Library/Saved Application State/com.openxdsp.uninstaller.savedState").deleteRecursively();

    // Delete matching user preferences
    juce::File userPrefs = userHome.getChildFile("Library/Preferences");
    if (userPrefs.isDirectory())
    {
        for (const auto& f : userPrefs.findChildFiles(juce::File::findFiles, false, "*openx*"))
            f.deleteFile();
    }

    // 2. Build system-level shell commands requiring root privileges
    juce::String systemCommands = "rm -rf ";

    // System VST3 plugins
    for (const auto& name : kPluginNames)
    {
        systemCommands += "'/Library/Audio/Plug-Ins/VST3/" + name + ".vst3' ";
    }

    // System AU plugins
    for (const auto& name : kPluginNames)
    {
        systemCommands += "'/Library/Audio/Plug-Ins/Components/" + name + ".component' ";
    }

    // Application Support & Receipts
    systemCommands += "'/Library/Application Support/Open-X' ";
    systemCommands += "'/Library/LaunchAgents/com.openxdsp'* ";
    systemCommands += "'/Library/LaunchDaemons/com.openxdsp'* ";
    systemCommands += "'/var/db/receipts/com.openxdsp'* 2>/dev/null || true; ";

    // Forget package receipts
    systemCommands += "pkgutil --pkgs 2>/dev/null | grep -iE 'openx|open-x' | while read -r p; do pkgutil --forget \"$p\" 2>/dev/null || true; done; ";

    // Reset AudioComponentRegistrar cache so DAWs immediately recognize plugins were removed
    systemCommands += "killall -9 AudioComponentRegistrar 2>/dev/null || true; ";

    // Cleanly delete the uninstaller app itself
    systemCommands += "rm -rf '/Applications/Open-X Uninstaller.app' ";
    if (currentAppPath.isNotEmpty() && currentAppPath.contains(".app"))
    {
        systemCommands += "'" + currentAppPath + "' ";
    }
    systemCommands += "2>/dev/null || true; ";

    // Launch background process to sweep uninstaller after process exit if any handle remained open
    systemCommands += "nohup /bin/sh -c 'sleep 1 && rm -rf \"/Applications/Open-X Uninstaller.app\" \"" + currentAppPath + "\"' >/dev/null 2>&1 & ";
    systemCommands += "exit 0";

    juce::String escapedCommands = systemCommands.replace("\\", "\\\\").replace("\"", "\\\"");
    juce::String appleScriptSource =
        "do shell script \"" + escapedCommands + "\" with administrator privileges";

    bool success = false;

    @autoreleasepool
    {
        NSString* src = [NSString stringWithUTF8String:appleScriptSource.toRawUTF8()];
        NSAppleScript* script = [[NSAppleScript alloc] initWithSource:src];
        NSDictionary* errorInfo = nil;
        NSAppleEventDescriptor* desc = [script executeAndReturnError:&errorInfo];

        if (errorInfo != nil)
        {
            NSNumber* errNum = [errorInfo objectForKey:NSAppleScriptErrorNumber];
            // User cancelled administrator authentication dialog (-128)
            if (errNum != nil && [errNum intValue] == -128)
            {
                return false;
            }
        }

        if (desc != nil || errorInfo == nil)
        {
            success = true;
        }
    }

    if (!success)
    {
        juce::ChildProcess process;
        juce::StringArray args;
        args.add("/usr/bin/osascript");
        args.add("-e");
        args.add(appleScriptSource);

        if (process.start(args))
        {
            process.waitForProcessToFinish(60000);
            success = (process.getExitCode() == 0);
        }
    }

    return success;
}

void UninstallerHelper::promptAndExecuteUninstall(juce::Component* parentComponent,
                                                  std::function<void(bool success)> onComplete)
{
    juce::String title = "Uninstall Open-X DSP Suite Completely";
    juce::String message =
        "Are you sure you want to completely uninstall Open-X DSP Suite from this system?\n\n"
        "This will remove:\n"
        "- All Open-X VST3 plugins (EQ-X, Comp-X, Limit-X, Verb-X, MB-X, DS-X, Gate-X)\n"
        "- All Open-X AudioUnit (AU) plugins\n"
        "- Presets, cache, and preference files\n"
        "- System package installer receipts\n"
        "- This uninstaller application\n\n"
        "This operation cannot be undone.";

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle(title)
        .withMessage(message)
        .withButton("Uninstall")
        .withButton("Cancel")
        .withParentComponent(parentComponent);

    juce::AlertWindow::showAsync(options, [onComplete](int result)
    {
        if (result != 1) // 1 = Yes / Uninstall
        {
            if (onComplete)
                onComplete(false);
            return;
        }

        bool success = executeUninstall();
        if (success)
        {
            auto okOptions = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("Uninstallation Complete")
                .withMessage("Open-X DSP Suite and all associated plugins, presets, and uninstaller have been removed.\n\nThe uninstaller will now close.")
                .withButton("OK");

            juce::AlertWindow::showAsync(okOptions, [onComplete](int)
            {
                if (onComplete)
                    onComplete(true);
                juce::JUCEApplicationBase::quit();
            });
        }
        else
        {
            auto errOptions = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Uninstallation Cancelled or Failed")
                .withMessage("Uninstallation was not completed. If administrator privileges were denied, please try again.")
                .withButton("OK");

            juce::AlertWindow::showAsync(errOptions, [onComplete](int)
            {
                if (onComplete)
                    onComplete(false);
            });
        }
    });
}

} // namespace openx::uninstaller
#endif
