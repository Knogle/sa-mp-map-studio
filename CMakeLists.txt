cmake_minimum_required(VERSION 3.21)

project(samp_map_studio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(APP_DISPLAY_NAME "SA:MP Map Studio")
set(APP_EXECUTABLE_NAME "samp-map-studio")
set(APP_DESKTOP_ID "sa-mp-map-studio")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
include(GNUInstallDirs)

qt_standard_project_setup()

qt_add_executable(samp_map_studio
    sampplotter.qrc
    src/editor_types.h
    src/img_archive.cpp
    src/img_archive.h
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/map_widget.cpp
    src/map_widget.h
    src/mapandreas_heightmap.cpp
    src/mapandreas_heightmap.h
    src/sa_map_loader.cpp
    src/sa_map_loader.h
    src/samp_zone_database.cpp
    src/samp_zone_database.h
    src/txd_reader.cpp
    src/txd_reader.h
)

set_target_properties(samp_map_studio PROPERTIES OUTPUT_NAME "${APP_EXECUTABLE_NAME}")

target_link_libraries(samp_map_studio PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

set(PACKAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/packaging")
file(MAKE_DIRECTORY "${PACKAGING_DIR}")

configure_file(packaging/sa-mp-map-studio.desktop.in
               "${PACKAGING_DIR}/${APP_DESKTOP_ID}.desktop"
               @ONLY)
configure_file(packaging/sa-mp-map-studio.metainfo.xml.in
               "${PACKAGING_DIR}/${APP_DESKTOP_ID}.metainfo.xml"
               @ONLY)

install(TARGETS samp_map_studio
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/samp_icon_32.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps
        RENAME ${APP_DESKTOP_ID}.png)
install(FILES "${PACKAGING_DIR}/${APP_DESKTOP_ID}.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES "${PACKAGING_DIR}/${APP_DESKTOP_ID}.metainfo.xml"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES "${PACKAGING_DIR}/${APP_DESKTOP_ID}.metainfo.xml"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
        RENAME ${APP_DESKTOP_ID}.appdata.xml)

qt_generate_deploy_app_script(
    TARGET samp_map_studio
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
