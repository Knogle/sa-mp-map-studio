# SA:MP Map Studio

Linux desktop tool to inspect the GTA San Andreas map and return `X, Y, Z`
coordinates for clicked positions.

It is designed as a practical SA:MP map editor for Linux: inspect positions,
place vehicles and player spawns, define gang zones, switch between radar and
terrain map views, and export Pawn or JSON data.

Current implementation details:

- GUI: Qt6 Widgets
- App icon: extracted from local `samp.exe`
- Preferred map source: GTA SA `models/gta3.img` radar tiles (`radar00..radar143`)
- Optional alternate map source: embedded aerial terrain map (`6000x6000`)
- Fallback map source: SA-MP `samaps.txd`
- Height source: MapAndreas `SAfull.hmap`
- SA:MP zone source: embedded zone dataset extracted from YSI `y_zonenames`
- Coordinate basis: `X` and `Y` span `[-3000, 3000)` and `Z` is read with the
  same truncation/indexing logic as `MapAndreas_FindZ_For2DCoord`.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/samp-map-studio
```

Controls:

- Inspect mode: left click picks `X, Y, Z`
- Hover: show the current SA:MP zone
- Mouse wheel: zoom in/out
- Right mouse drag: pan while zoomed
- `+`, `-`, `Reset Zoom`: GUI zoom controls

Editor features:

- `Place Vehicle`: left click creates `CreateVehicle(...)` entries
- `Set Player Spawn`: left click creates `AddPlayerClassEx(...)` entries
- `Define Gang Zone`: left-drag creates `GangZoneCreate(...)` rectangles
- Map mode switch: `Radar Map` vs `Terrain Map`
- Item list with map overlays and selection highlighting
- Export tab with Pawn and JSON output
- Copy selected Pawn snippet, copy full export, or save export to file

## Project Layout

- `src/`: application sources
- `data/`: bundled assets such as icons, terrain map and zone data
- `packaging/`: Linux desktop metadata templates
- `scripts/build_appimage.sh`: AppDir/AppImage build helper

## Native Dependencies

- CMake 3.21+
- C++20 compiler
- Qt 6 Widgets development files

Fedora packages:

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qttools-devel desktop-file-utils appstream
```

Optional paths:

```bash
./build/samp-map-studio --img /path/to/models/gta3.img --terrain-map /path/to/terrain.jpg --txd /path/to/samaps.txd --hmap /path/to/SAfull.hmap
```

Debug/export helpers:

```bash
./build/samp-map-studio --dump-textures out_dir --export-map out.png
```

## AppImage

The project now includes Linux desktop metadata and a build script for AppImage
packaging.

Requirement:

- `appimagetool`, or set `APPIMAGETOOL` to an upstream AppImage binary

Build the AppImage:

```bash
./scripts/build_appimage.sh
```

Output:

```bash
build/SA-MP_Map_Studio-$(uname -m).AppImage
```

Only stage the `AppDir`:

```bash
./scripts/build_appimage.sh --appdir-only
```

## Fedora Toolbox

The project builds in a Fedora toolbox such as `devbuild`.

Install the required packages inside the toolbox:

```bash
podman exec --user root devbuild bash -lc 'dnf install -y cmake gcc-c++ qt6-qtbase-devel qt6-qttools-devel desktop-file-utils appstream'
```

Build inside the toolbox:

```bash
toolbox run -c devbuild bash -lc 'cmake -S /home/chairman/Projects/sampplotter -B /home/chairman/Projects/sampplotter/build-devbuild && cmake --build /home/chairman/Projects/sampplotter/build-devbuild -j'
```

Build the AppImage inside the toolbox with an explicit `appimagetool` binary:

```bash
toolbox run -c devbuild bash -lc "APPIMAGETOOL=/home/chairman/Projects/sampplotter/tools/appimagetool-x86_64.AppImage /home/chairman/Projects/sampplotter/scripts/build_appimage.sh /home/chairman/Projects/sampplotter/build-devbuild /home/chairman/Projects/sampplotter/build-devbuild/AppDir /home/chairman/Projects/sampplotter/build-devbuild/SA-MP_Map_Studio-x86_64.AppImage"
```
