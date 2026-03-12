#!/usr/bin/env bash
set -euo pipefail

APPDIR_ONLY=0
if [[ "${1:-}" == "--appdir-only" ]]; then
    APPDIR_ONLY=1
    shift
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build}"
APPDIR="${2:-$BUILD_DIR/AppDir}"
APPIMAGE_PATH="${3:-$BUILD_DIR/SA-MP_Map_Studio-$(uname -m).AppImage}"
APP_ID="sa-mp-map-studio"
QT_PLUGIN_DIR="$(qtpaths6 --query QT_INSTALL_PLUGINS)"
APPIMAGETOOL_ARGS="${APPIMAGETOOL_ARGS:--n}"

declare -A COPIED_LIBS=()

skip_lib() {
    local base
    base="$(basename "$1")"
    case "${base}" in
        linux-vdso.so.*|ld-linux*.so*|libc.so.*|libm.so.*|libpthread.so.*|libdl.so.*|librt.so.*)
            return 0
            ;;
        libGL.so.*|libGLX.so.*|libGLdispatch.so.*|libEGL.so.*|libOpenGL.so.*)
            return 0
            ;;
        libX11.so.*|libXext.so.*|libXau.so.*|libxcb.so.*|libdrm.so.*|libwayland-*.so.*|libgbm.so.*)
            return 0
            ;;
    esac
    return 1
}

bundle_deps() {
    local input="$1"
    while IFS= read -r dep; do
        [[ -n "${dep}" ]] || continue
        [[ -f "${dep}" ]] || continue
        if skip_lib "${dep}"; then
            continue
        fi

        if [[ -z "${COPIED_LIBS[${dep}]:-}" ]]; then
            COPIED_LIBS["${dep}"]=1
            cp -L "${dep}" "${APPDIR}/usr/lib64/"
            chmod 0755 "${APPDIR}/usr/lib64/$(basename "${dep}")"
            bundle_deps "${dep}"
        fi
    done < <(ldd "${input}" | awk '/=> \// { print $3 } /^\// { print $1 }')
}

copy_plugin() {
    local subdir="$1"
    local filename="$2"
    local src_path="${QT_PLUGIN_DIR}/${subdir}/${filename}"
    local dst_dir="${APPDIR}/usr/lib64/qt6/plugins/${subdir}"

    if [[ ! -f "${src_path}" ]]; then
        return
    fi

    mkdir -p "${dst_dir}"
    cp -L "${src_path}" "${dst_dir}/"
    chmod 0755 "${dst_dir}/${filename}"
    bundle_deps "${dst_dir}/${filename}"
}

APPIMAGETOOL_BIN="${APPIMAGETOOL:-$(command -v appimagetool || true)}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j

rm -rf "${APPDIR}"
cmake --install "${BUILD_DIR}" --prefix "${APPDIR}/usr"
mkdir -p "${APPDIR}/usr/lib64/qt6/plugins"

cp "${BUILD_DIR}/packaging/${APP_ID}.desktop" "${APPDIR}/${APP_ID}.desktop"
cp "${ROOT_DIR}/data/samp_icon_32.png" "${APPDIR}/${APP_ID}.png"
cp "${ROOT_DIR}/data/samp_icon_32.png" "${APPDIR}/.DirIcon"

bundle_deps "${APPDIR}/usr/bin/samp-map-studio"
copy_plugin "platforms" "libqxcb.so"
copy_plugin "platforms" "libqoffscreen.so"
copy_plugin "imageformats" "libqjpeg.so"
copy_plugin "imageformats" "libqgif.so"
copy_plugin "imageformats" "libqico.so"
copy_plugin "xcbglintegrations" "libqxcb-egl-integration.so"
copy_plugin "xcbglintegrations" "libqxcb-glx-integration.so"

cat > "${APPDIR}/AppRun" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export APPDIR="${SELF_DIR}"
export PATH="${SELF_DIR}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${SELF_DIR}/usr/lib64:${SELF_DIR}/usr/lib:${SELF_DIR}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${SELF_DIR}/usr/lib64/qt6/plugins"
export QML2_IMPORT_PATH="${SELF_DIR}/usr/lib64/qt6/qml"
exec "${SELF_DIR}/usr/bin/samp-map-studio" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

if [[ "${APPDIR_ONLY}" -eq 1 ]]; then
    echo "Prepared ${APPDIR}"
    exit 0
fi

if [[ -z "${APPIMAGETOOL_BIN}" ]]; then
    echo "appimagetool not found. Install it or set APPIMAGETOOL=/path/to/appimagetool." >&2
    exit 1
fi

ARCH="${ARCH:-$(uname -m)}" "${APPIMAGETOOL_BIN}" ${APPIMAGETOOL_ARGS} "${APPDIR}" "${APPIMAGE_PATH}"
echo "Created ${APPIMAGE_PATH}"
