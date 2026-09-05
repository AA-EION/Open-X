#!/usr/bin/env python3
"""
Open-X DSP Suite - macOS PKG Packager
Creates a single universal macOS installer package (.pkg) that deploys:
- All Open-X VST3 plugins to /Library/Audio/Plug-Ins/VST3
- All Open-X AudioUnit (AU) plugins to /Library/Audio/Plug-Ins/Components
- The Open-X Uninstaller GUI application to /Applications

Ad-hoc signs all Mach-O binaries and bundles with hardened runtime for seamless
Gatekeeper execution on Apple Silicon and Intel systems.
"""

import os
import sys
import shutil
import subprocess
import xml.etree.ElementTree as ET

PLUGIN_NAMES = [
    "EQ-X",
    "Comp-X",
    "Limit-X",
    "Verb-X",
    "MB-X",
    "DS-X",
    "Gate-X"
]

def run_cmd(cmd, check=True):
    print(f"[CMD] {cmd}")
    res = subprocess.run(cmd, shell=True)
    if check and res.returncode != 0:
        raise RuntimeError(f"Command failed with exit code {res.returncode}: {cmd}")
    return res.returncode

def ad_hoc_sign(path):
    """Ad-hoc signs bundle with hardened runtime for Gatekeeper compatibility."""
    if not os.path.exists(path):
        return
    print(f"[SIGN] Ad-hoc signing {path}...")
    run_cmd(f'codesign --force --deep -s - --options=runtime "{path}"', check=False)

def find_item_in_build(search_dirs, filename):
    for sdir in search_dirs:
        if not os.path.isdir(sdir):
            continue
        for root, dirs, files in os.walk(sdir):
            for d in dirs:
                if d == filename:
                    return os.path.join(root, d)
            for f in files:
                if f == filename:
                    return os.path.join(root, f)
    return None

def main():
    build_dir = os.getenv("BUILD_DIR", "build")
    output_dir = os.getenv("OUTPUT_DIR", "staging")
    version = os.getenv("VERSION", "1.0.0")
    product_name = os.getenv("PRODUCT_NAME", "Open-X DSP Suite")
    bundle_id_base = os.getenv("BUNDLE_ID", "com.openxdsp.openxsuite")
    macos_arch = os.getenv("MACOS_ARCH", "arm64,x86_64")

    os.makedirs(output_dir, exist_ok=True)
    tmp_pkg_dir = os.path.join(output_dir, "pkg_components")
    payload_dir = os.path.join(output_dir, "pkg_payloads")
    shutil.rmtree(tmp_pkg_dir, ignore_errors=True)
    shutil.rmtree(payload_dir, ignore_errors=True)
    os.makedirs(tmp_pkg_dir, exist_ok=True)
    os.makedirs(payload_dir, exist_ok=True)

    search_dirs = [
        build_dir,
        os.path.join(build_dir, "plugins"),
        os.path.join(build_dir, "uninstaller"),
        os.path.join(build_dir, "uninstaller/OpenXUninstaller_artefacts"),
        "staging"
    ]

    # 1. Stage and sign VST3 plugins
    vst3_payload = os.path.join(payload_dir, "vst3")
    os.makedirs(vst3_payload, exist_ok=True)
    found_vst3 = 0
    for name in PLUGIN_NAMES:
        bundle_name = f"{name}.vst3"
        bundle_path = find_item_in_build(search_dirs, bundle_name)
        if bundle_path:
            dest = os.path.join(vst3_payload, bundle_name)
            shutil.copytree(bundle_path, dest, dirs_exist_ok=True)
            ad_hoc_sign(dest)
            found_vst3 += 1
            print(f"[FOUND] VST3: {bundle_path} -> {dest}")
        else:
            print(f"[WARN] Could not find VST3 bundle for {name}")

    # 2. Stage and sign AU plugins
    au_payload = os.path.join(payload_dir, "au")
    os.makedirs(au_payload, exist_ok=True)
    found_au = 0
    for name in PLUGIN_NAMES:
        bundle_name = f"{name}.component"
        bundle_path = find_item_in_build(search_dirs, bundle_name)
        if bundle_path:
            dest = os.path.join(au_payload, bundle_name)
            shutil.copytree(bundle_path, dest, dirs_exist_ok=True)
            ad_hoc_sign(dest)
            found_au += 1
            print(f"[FOUND] AU: {bundle_path} -> {dest}")
        else:
            print(f"[WARN] Could not find AU bundle for {name}")

    # 3. Stage and sign Uninstaller application
    app_payload = os.path.join(payload_dir, "app")
    os.makedirs(app_payload, exist_ok=True)
    uninstaller_names = ["Open-X Uninstaller.app", "OpenXUninstaller.app"]
    uninstaller_path = None
    for uname in uninstaller_names:
        uninstaller_path = find_item_in_build(search_dirs, uname)
        if uninstaller_path:
            break

    if uninstaller_path:
        dest_app = os.path.join(app_payload, "Open-X Uninstaller.app")
        shutil.copytree(uninstaller_path, dest_app, dirs_exist_ok=True)
        ad_hoc_sign(dest_app)
        print(f"[FOUND] Uninstaller App: {uninstaller_path} -> {dest_app}")
    else:
        print("[WARN] Could not find Open-X Uninstaller.app in build directory!")

    # 4. Build Component PKGs with pkgbuild
    component_pkgs = []

    if found_vst3 > 0:
        vst3_pkg = os.path.join(tmp_pkg_dir, "openx_vst3.pkg")
        pkg_id = f"{bundle_id_base}.vst3"
        run_cmd(
            f'pkgbuild --root "{vst3_payload}" '
            f'--install-location "/Library/Audio/Plug-Ins/VST3" '
            f'--identifier "{pkg_id}" '
            f'--version "{version}" '
            f'"{vst3_pkg}"'
        )
        component_pkgs.append({
            "id": pkg_id,
            "file": "openx_vst3.pkg",
            "title": f"{product_name} VST3 Plugins",
            "desc": "Installs 7 universal VST3 audio plugins (EQ-X, Comp-X, Limit-X, Verb-X, MB-X, DS-X, Gate-X)."
        })

    if found_au > 0:
        au_pkg = os.path.join(tmp_pkg_dir, "openx_au.pkg")
        pkg_id = f"{bundle_id_base}.au"
        run_cmd(
            f'pkgbuild --root "{au_payload}" '
            f'--install-location "/Library/Audio/Plug-Ins/Components" '
            f'--identifier "{pkg_id}" '
            f'--version "{version}" '
            f'"{au_pkg}"'
        )
        component_pkgs.append({
            "id": pkg_id,
            "file": "openx_au.pkg",
            "title": f"{product_name} AudioUnit (AU) Plugins",
            "desc": "Installs 7 universal AudioUnit plugins for macOS DAWs (Logic Pro, GarageBand, Ableton, Reaper)."
        })

    if os.path.exists(os.path.join(app_payload, "Open-X Uninstaller.app")):
        app_pkg = os.path.join(tmp_pkg_dir, "openx_uninstaller.pkg")
        pkg_id = f"{bundle_id_base}.uninstaller"
        run_cmd(
            f'pkgbuild --root "{app_payload}" '
            f'--install-location "/Applications" '
            f'--identifier "{pkg_id}" '
            f'--version "{version}" '
            f'"{app_pkg}"'
        )
        component_pkgs.append({
            "id": pkg_id,
            "file": "openx_uninstaller.pkg",
            "title": "Open-X Uninstaller Application",
            "desc": "Installs Open-X Uninstaller.app in /Applications for clean single-click bundle uninstallation."
        })

    # 5. Generate distribution.xml for productbuild
    root = ET.Element("installer-gui-script", minSpecVersion="1")
    title_elem = ET.SubElement(root, "title")
    title_elem.text = f"{product_name} {version}"

    license_file = "packaging/resources/license.rtf"
    if os.path.isfile(license_file):
        ET.SubElement(root, "license", file=license_file, mime_type="application/rtf")

    ET.SubElement(root, "options",
                  customize="always",
                  rootVolumeOnly="true",
                  hostArchitectures=macos_arch)

    ET.SubElement(root, "domain",
                  enable_anywhere="false",
                  enable_currentUserHome="false",
                  enable_localSystem="true")

    outline = ET.SubElement(root, "choices-outline")

    for comp in component_pkgs:
        c_id = comp["id"]
        pkg_ref = ET.SubElement(root, "pkg-ref", id=c_id, version=version, onConclusion="none")
        pkg_ref.text = comp["file"]

        choice = ET.SubElement(root, "choice",
                                id=c_id,
                                visible="true",
                                start_selected="true",
                                title=comp["title"],
                                description=comp["desc"])
        ET.SubElement(choice, "pkg-ref", id=c_id)
        ET.SubElement(outline, "line", choice=c_id)

    dist_xml_path = os.path.join(output_dir, "distribution.xml")
    ET.indent(root, space="    ", level=0)
    tree = ET.ElementTree(root)
    tree.write(dist_xml_path, encoding="utf-8", xml_declaration=True)
    print(f"[DIST] Created distribution xml: {dist_xml_path}")

    # 6. Build final bundle package with productbuild
    final_pkg_name = f"Open-X-Suite-macOS.pkg"
    final_pkg_path = os.path.join(output_dir, final_pkg_name)

    resources_dir = "packaging/resources"
    cmd = [
        "productbuild",
        "--distribution", dist_xml_path,
        "--package-path", tmp_pkg_dir,
        final_pkg_path
    ]
    if os.path.isdir(resources_dir):
        cmd.extend(["--resources", resources_dir])

    print(f"[BUILD] Building final bundle package: {final_pkg_path}...")
    run_cmd(" ".join(f'"{c}"' if " " in c else c for c in cmd))

    print(f"\n[SUCCESS] Successfully generated single bundle macOS installer:")
    print(f" -> {final_pkg_path}\n")
    return 0

if __name__ == "__main__":
    sys.exit(main())
