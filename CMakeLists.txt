cmake_minimum_required(VERSION 3.22)
project(spatial_vms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PahoMqttCpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/TopBar.cpp
    src/TiltBanner.cpp
    src/CameraTile.cpp
    src/TopViewWidget.cpp
    src/TopViewPanel.cpp
    src/StatusBar.cpp
    src/CalibrationTab.cpp
    src/DevicesTab.cpp
    src/DatasetTab.cpp
    src/EventLogTab.cpp
    src/MqttBridge.cpp
    src/DemoBridge.cpp
    src/RtspDecoder.cpp
    src/RtspSource.cpp
)

set(HEADERS
    src/Theme.h
    src/Models.h
    src/DataBridge.h
    src/MainWindow.h
    src/TopBar.h
    src/TiltBanner.h
    src/CameraTile.h
    src/TopViewWidget.h
    src/TopViewPanel.h
    src/StatusBar.h
    src/CalibrationTab.h
    src/DevicesTab.h
    src/DatasetTab.h
    src/EventLogTab.h
    src/MqttBridge.h
    src/DemoBridge.h
    src/RtspDecoder.h
    src/RtspSource.h
)

if(APPLE)
    set(MACOSX_BUNDLE_FLAG MACOSX_BUNDLE)
    set(APP_ICON_SOURCES resources/AppIcon.icns)
    set_source_files_properties(resources/AppIcon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
elseif(WIN32)
    set(MACOSX_BUNDLE_FLAG WIN32)   # 콘솔 창 없이 뜨는 일반 GUI exe
    set(APP_ICON_SOURCES resources/app.rc)
else()
    set(MACOSX_BUNDLE_FLAG)
    set(APP_ICON_SOURCES)
endif()

qt_add_executable(spatial_vms ${MACOSX_BUNDLE_FLAG} ${SOURCES} ${HEADERS} ${APP_ICON_SOURCES})

set_target_properties(spatial_vms PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "SPATIAL-VMS"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.veda4th.spatialvms"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
)

target_include_directories(spatial_vms PRIVATE src)

target_link_libraries(spatial_vms PRIVATE
    Qt6::Widgets
    PahoMqttCpp::paho-mqttpp3
    PkgConfig::FFMPEG
)
