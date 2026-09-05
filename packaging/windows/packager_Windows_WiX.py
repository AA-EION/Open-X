#!/usr/bin/env python3
"""
Open-X DSP Suite - Windows WiX MSI Packager
Generates a WiX v4/v5 installer configuration (.wxs) and builds a single bundle MSI installer.
Features:
- Deploys all 7 Open-X VST3 plugins to CommonFiles64Folder/VST3
- Supports universal multi-architecture VST3 bundles (Contents/x86_64-win and Contents/arm64-win)
- Places an uninstaller shortcut in Start Menu / Program Files (msiexec /x)
- Registers clean full-bundle uninstallation in Windows Add/Remove Programs (Installed Apps)
- Uses WixUI_FeatureTree with GPL-3.0 RTF license agreement
"""

import os
import sys
import uuid
import hashlib
import shutil
import subprocess

NAMESPACE_GUID = uuid.UUID('9f8b4a2c-3d1e-4f5a-8b7c-6e9d0a1b2c3d')

PLUGIN_NAMES = [
    "EQ-X",
    "Comp-X",
    "Limit-X",
    "Verb-X",
    "MB-X",
    "DS-X",
    "Gate-X"
]

def get_guid(string_input):
    return str(uuid.uuid5(NAMESPACE_GUID, string_input)).upper()

def get_wix_id(string_input):
    h = hashlib.md5(string_input.encode('utf-8')).hexdigest().upper()
    return "ID_" + h

def escape_xml(s):
    return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")

def write_dir_recursive(file_handle, current_os_path, parent_wix_id, component_list, prefix):
    try:
        items = sorted(os.listdir(current_os_path))
    except OSError:
        return

    files = [i for i in items if os.path.isfile(os.path.join(current_os_path, i))]
    dirs = [i for i in items if os.path.isdir(os.path.join(current_os_path, i))]
    files = [f for f in files if not f.endswith(".ilk") and not f.endswith(".pdb")]

    for filename in files:
        full_path = os.path.join(current_os_path, filename)
        comp_id = get_wix_id(f"COMP_{prefix}_{full_path}")
        file_id = get_wix_id(f"FILE_{prefix}_{full_path}")
        component_list.append(comp_id)

        file_handle.write(f'            <Component Id="{comp_id}" Guid="{get_guid(comp_id)}">\n')
        file_handle.write(f'                <File Id="{file_id}" Source="{full_path}" KeyPath="yes" />\n')
        file_handle.write('            </Component>\n')

    for dirname in dirs:
        full_path = os.path.join(current_os_path, dirname)
        dir_id = get_wix_id(f"DIR_{prefix}_{full_path}")
        clean_dirname = escape_xml(dirname)
        file_handle.write(f'            <Directory Id="{dir_id}" Name="{clean_dirname}">\n')
        write_dir_recursive(file_handle, full_path, dir_id, component_list, prefix)
        file_handle.write('            </Directory>\n')

def main():
    product_name = os.getenv("PRODUCT_NAME", "Open-X DSP Suite")
    version = os.getenv("VERSION", "1.0.0")
    publisher = os.getenv("COMPANY_NAME", "Open-X DSP")
    staging_dir = os.getenv("STAGING_DIR", "staging")
    build_dir = os.getenv("BUILD_DIR", "build")
    wxs_output = os.getenv("WXS_OUTPUT", "packaging/windows/installer.wxs")
    output_msi = os.getenv("OUTPUT_MSI", os.path.join(staging_dir, "Open-X-Suite-Windows.msi"))

    os.makedirs(os.path.dirname(wxs_output), exist_ok=True)
    os.makedirs(staging_dir, exist_ok=True)

    # Search paths for x64 and arm64 builds
    search_dirs = [
        staging_dir,
        "build-x64",
        "build-arm64",
        os.path.join(build_dir, "plugins"),
        build_dir
    ]

    # Staging universal VST3 bundles
    plugin_paths = {}
    for name in PLUGIN_NAMES:
        bundle_name = f"{name}.vst3"
        staged_bundle = os.path.join(staging_dir, bundle_name)
        os.makedirs(staged_bundle, exist_ok=True)

        found_any = False

        # Look for x64 build
        for sdir in ["build-x64", build_dir, os.path.join(build_dir, "plugins")]:
            if not os.path.isdir(sdir): continue
            for root, dirs, _ in os.walk(sdir):
                if bundle_name in dirs:
                    src_bundle = os.path.join(root, bundle_name)
                    # Copy contents
                    x64_contents = os.path.join(src_bundle, "Contents", "x86_64-win")
                    if os.path.isdir(x64_contents):
                        dest_x64 = os.path.join(staged_bundle, "Contents", "x86_64-win")
                        os.makedirs(dest_x64, exist_ok=True)
                        shutil.copytree(x64_contents, dest_x64, dirs_exist_ok=True)
                        found_any = True
                    # Also copy Resources if present
                    res_dir = os.path.join(src_bundle, "Contents", "Resources")
                    if os.path.isdir(res_dir):
                        dest_res = os.path.join(staged_bundle, "Contents", "Resources")
                        shutil.copytree(res_dir, dest_res, dirs_exist_ok=True)
                    # If not a bundle directory structure, copy entire bundle
                    if not os.path.isdir(os.path.join(src_bundle, "Contents")):
                        shutil.copytree(src_bundle, staged_bundle, dirs_exist_ok=True)
                        found_any = True
                    break
            if found_any: break

        # Look for arm64 build
        for sdir in ["build-arm64"]:
            if not os.path.isdir(sdir): continue
            for root, dirs, _ in os.walk(sdir):
                if bundle_name in dirs:
                    src_bundle = os.path.join(root, bundle_name)
                    arm64_contents = os.path.join(src_bundle, "Contents", "arm64-win")
                    if os.path.isdir(arm64_contents):
                        dest_arm64 = os.path.join(staged_bundle, "Contents", "arm64-win")
                        os.makedirs(dest_arm64, exist_ok=True)
                        shutil.copytree(arm64_contents, dest_arm64, dirs_exist_ok=True)
                        found_any = True
                        print(f"[UNIVERSAL] Merged arm64-win for {name}")
                    break

        # Fallback search if staging didn't get contents
        if os.path.isdir(staged_bundle) and len(os.listdir(staged_bundle)) > 0:
            plugin_paths[name] = staged_bundle
            print(f"[FOUND/STAGED] {name}: {staged_bundle}")
        else:
            # Fallback to direct search
            found_path = None
            for sdir in search_dirs:
                if not os.path.isdir(sdir): continue
                for root, dirs, files in os.walk(sdir):
                    if bundle_name in dirs:
                        found_path = os.path.join(root, bundle_name)
                        break
                    if bundle_name in files:
                        found_path = os.path.join(root, bundle_name)
                        break
                if found_path: break
            if found_path:
                plugin_paths[name] = found_path
                print(f"[FOUND] {name}: {found_path}")
            else:
                print(f"[WARN] Could not find VST3 bundle for {name}")

    upgrade_code = get_guid("OpenX_DSP_Suite_UpgradeCode")

    with open(wxs_output, "w", encoding="utf-8") as f:
        f.write('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs" xmlns:ui="http://wixtoolset.org/schemas/v4/wxs/ui">\n')
        f.write(f'    <Package Name="{escape_xml(product_name)}" Manufacturer="{escape_xml(publisher)}" '
                f'Version="{version}" UpgradeCode="{upgrade_code}" Scope="perMachine" Compressed="yes">\n')
        f.write('        <MajorUpgrade AllowDowngrades="yes" Schedule="afterInstallInitialize" />\n')
        f.write('        <MediaTemplate EmbedCab="yes" />\n')

        # Add/Remove Programs Properties
        f.write('        <Property Id="ARPHELPLINK" Value="https://github.com/AA-EION/Open-X" />\n')
        f.write('        <Property Id="ARPURLINFOABOUT" Value="https://github.com/AA-EION/Open-X" />\n')
        f.write('        <Property Id="ARPCOMMENTS" Value="Open-X DSP Professional Audio Plugin Suite" />\n')

        # Directory structure
        f.write('        <StandardDirectory Id="CommonFiles64Folder">\n')
        f.write('            <Directory Id="VST3DIR" Name="VST3">\n')

        features = {}
        for name, src_path in plugin_paths.items():
            feat_id = f"Feature_{name.replace('-', '_')}"
            bundle_dir_id = get_wix_id(f"DIR_BUNDLE_{name}")
            bundle_name = f"{name}.vst3"
            components = []

            f.write(f'                <Directory Id="{bundle_dir_id}" Name="{bundle_name}">\n')
            if os.path.isdir(src_path):
                write_dir_recursive(f, src_path, bundle_dir_id, components, name)
            else:
                comp_id = get_wix_id(f"COMP_FILE_{name}")
                file_id = get_wix_id(f"FILE_FILE_{name}")
                components.append(comp_id)
                f.write(f'                    <Component Id="{comp_id}" Guid="{get_guid(comp_id)}">\n')
                f.write(f'                        <File Id="{file_id}" Source="{src_path}" KeyPath="yes" />\n')
                f.write('                    </Component>\n')
            f.write('                </Directory>\n')

            features[feat_id] = {
                "name": name,
                "title": f"{name} VST3 Plugin",
                "components": components
            }

        f.write('            </Directory>\n')
        f.write('        </StandardDirectory>\n')

        # Start Menu uninstaller shortcut placement
        f.write('        <StandardDirectory Id="ProgramMenuFolder">\n')
        f.write('            <Directory Id="CompanyMenuDir" Name="Open-X DSP">\n')
        uninstaller_comp_id = "Comp_UninstallerShortcut"
        f.write(f'                <Component Id="{uninstaller_comp_id}" Guid="{get_guid(uninstaller_comp_id)}">\n')
        f.write('                    <Shortcut Id="UninstallOpenXSuite" Name="Uninstall Open-X DSP Suite" '
                'Description="Completely uninstalls Open-X DSP Suite and all associated plugins" '
                'Target="[SystemFolder]msiexec.exe" Arguments="/x [ProductCode]" />\n')
        f.write('                    <RemoveFolder Id="RemoveCompanyMenuDir" Directory="CompanyMenuDir" On="uninstall" />\n')
        f.write('                    <RegistryValue Root="HKCU" Key="Software\\OpenX\\OpenXSuite" Name="installed" Type="integer" Value="1" KeyPath="yes" />\n')
        f.write('                </Component>\n')
        f.write('            </Directory>\n')
        f.write('        </StandardDirectory>\n')

        # Feature Tree
        f.write('        <Feature Id="Complete" Title="Open-X DSP Suite" Display="expand" Level="1">\n')
        f.write(f'            <ComponentRef Id="{uninstaller_comp_id}" />\n')
        for feat_id, data in features.items():
            f.write(f'            <Feature Id="{feat_id}" Title="{data["title"]}" Level="1">\n')
            for comp_id in data["components"]:
                f.write(f'                <ComponentRef Id="{comp_id}" />\n')
            f.write('            </Feature>\n')
        f.write('        </Feature>\n')

        # UI Configuration
        license_path = "packaging/resources/license.rtf"
        if os.path.exists(license_path):
            f.write(f'        <WixVariable Id="WixUILicenseRtf" Value="{license_path}" />\n')

        f.write('        <UI>\n')
        f.write('            <ui:WixUI Id="WixUI_FeatureTree" />\n')
        f.write('            <UIRef Id="WixUI_ErrorProgressText" />\n')
        f.write('        </UI>\n')

        f.write('    </Package>\n')
        f.write('</Wix>\n')

    print(f"[WIX] Generated WiX manifest at {wxs_output}")

    # Build MSI if wix CLI is present
    if shutil.which("wix"):
        print(f"[BUILD] Running wix build...")
        cmd = [
            "wix", "build", wxs_output,
            "-ext", "WixToolset.UI.wixext",
            "-arch", "x64",
            "-loc", "packaging/windows/overrides.wxl",
            "-o", output_msi
        ]
        res = subprocess.run(cmd)
        if res.returncode == 0:
            print(f"[SUCCESS] Built MSI installer: {output_msi}")
        else:
            print(f"[WARN] wix build returned {res.returncode}")
    else:
        print("[INFO] 'wix' command not found in PATH. Skipping immediate build (will run on CI runner).")

if __name__ == "__main__":
    main()
