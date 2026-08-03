cmake_minimum_required(VERSION 3.22)
project(spatial_vms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PahoMqttCpp REQUIRED)

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
)

qt_add_executable(spatial_vms ${SOURCES} ${HEADERS})

target_include_directories(spatial_vms PRIVATE src)

target_link_libraries(spatial_vms PRIVATE
    Qt6::Widgets
    PahoMqttCpp::paho-mqttpp3
)
