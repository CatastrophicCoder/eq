#!/bin/bash
#
# Builds Spectral Fault in Release and packages it for installation on another Mac:
#   build/…/artefacts/SpectralFault-<version>.pkg   installer; AU, VST3 and Standalone selectable
#   build/…/artefacts/SpectralFault-<version>.dmg   the Standalone app and a read-me
#
# The bundles are ad-hoc signed (decision 2026-09-29): enough for Apple silicon to run the code,
# but it does not identify the maker, so the first use on another Mac needs System Settings >
# Privacy & Security > Open Anyway. packaging/README.md and the installer's welcome page explain it.
#
# SPECTRALFAULT_BUILD_DIR points at an existing Release tree (CI) instead of a separate one.
#
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="${SPECTRALFAULT_BUILD_DIR:-$root/build/package-release}"
staging="$build/package"
out="$build/artefacts"
name="SpectralFault"
product="Spectral Fault"
identifier="com.catastrophicaudio.spectralfault"

version="$(sed -n 's/^project(SpectralFault VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
: "${version:?could not read the version from CMakeLists.txt}"

echo "==> $product $version"

# --- build ---------------------------------------------------------------------------------------
# Without tests, and without copying into ~/Library, so a local run leaves the installed Debug
# plugin alone. A tree configured with tests (CI) keeps them; only the plugin targets are built here.
if [[ -z "${SPECTRALFAULT_BUILD_DIR:-}" ]]; then
    cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DSPECTRALFAULT_BUILD_TESTS=OFF -DSPECTRALFAULT_COPY_PLUGIN=OFF > /dev/null
fi
cmake --build "$build" --target "${name}_AU" "${name}_VST3" "${name}_Standalone" > /dev/null

artefacts="$build/${name}_artefacts/Release"
au="$artefacts/AU/$product.component"
vst3="$artefacts/VST3/$product.vst3"
app="$artefacts/Standalone/$product.app"

# --- sign ----------------------------------------------------------------------------------------
# Ad-hoc ("-"), each bundle on its own (not --deep, which Apple deprecated for signing).
for bundle in "$au" "$vst3" "$app"; do
    echo "==> signing $(basename "$bundle")"
    codesign --force --sign - --timestamp=none "$bundle"
    codesign --verify --strict "$bundle"
done

# --- stage ---------------------------------------------------------------------------------------
rm -rf "$staging" "$out"
mkdir -p "$staging/au/Library/Audio/Plug-Ins/Components" \
         "$staging/vst3/Library/Audio/Plug-Ins/VST3" \
         "$staging/app/Applications" \
         "$staging/pkgs" "$staging/resources" "$out"

cp -R "$au"   "$staging/au/Library/Audio/Plug-Ins/Components/"
cp -R "$vst3" "$staging/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$app"  "$staging/app/Applications/"

# Clear extended attributes (e.g. quarantine) from the payload; pkgbuild stores any that remain as
# AppleDouble entries, which the Installer turns back into attributes. macOS's protected
# com.apple.provenance cannot be cleared and may remain. (Code signatures live inside the bundles.)
xattr -cr "$staging"
for bundle in "$staging"/au/Library/Audio/Plug-Ins/Components/*.component \
              "$staging"/vst3/Library/Audio/Plug-Ins/VST3/*.vst3 \
              "$staging"/app/Applications/*.app; do
    codesign --verify --strict "$bundle"
done

cp "$root/packaging/resources/"* "$staging/resources/"
cp "$root/LICENSE" "$staging/resources/license.txt"

# --- component packages --------------------------------------------------------------------------
# One per format, so each can be chosen in the installer. Relocation is turned off: all three
# bundles share JUCE's single bundle ID, and a relocatable package would install over whichever
# bundle with that ID it finds (the Standalone app landing on the AU) instead of where it says.
build_component() {
    local root="$1" id="$2" pkg="$3"
    local plist="$staging/$(basename "$pkg" .pkg)-component.plist"

    pkgbuild --analyze --root "$root" "$plist" > /dev/null

    local entries
    entries="$(/usr/libexec/PlistBuddy -c "Print" "$plist" | grep -c "BundleIsRelocatable" || true)"
    for ((i = 0; i < entries; ++i)); do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$plist"
    done

    pkgbuild --root "$root" --identifier "$id" --version "$version" \
             --component-plist "$plist" --install-location / "$pkg" > /dev/null
}

build_component "$staging/au"   "$identifier.au"         "$staging/pkgs/au.pkg"
build_component "$staging/vst3" "$identifier.vst3"       "$staging/pkgs/vst3.pkg"
build_component "$staging/app"  "$identifier.standalone" "$staging/pkgs/app.pkg"

# --- installer -----------------------------------------------------------------------------------
sed "s/@VERSION@/$version/g" "$root/packaging/distribution.xml" > "$staging/distribution.xml"
sed "s/@VERSION@/$version/g" "$root/packaging/resources/welcome.html" > "$staging/resources/welcome.html"

productbuild --distribution "$staging/distribution.xml" \
             --package-path "$staging/pkgs" \
             --resources "$staging/resources" \
             "$out/$name-$version.pkg" > /dev/null

# --- disk image with the Standalone app ------------------------------------------------------------
dmg="$staging/dmg"
mkdir -p "$dmg"
cp -R "$app" "$dmg/"
cp "$root/packaging/README.md" "$dmg/Read me first.txt"
ln -s /Applications "$dmg/Applications"
hdiutil create -volname "$product $version" -srcfolder "$dmg" -ov -quiet -format UDZO "$out/$name-$version.dmg"

echo
echo "==> built:"
ls -lh "$out" | awk 'NR > 1 { print "    " $9 "  " $5 }'
echo
echo "    Ad-hoc signed: the first use on another Mac needs"
echo "    System Settings > Privacy & Security > Open Anyway (see packaging/README.md)."
