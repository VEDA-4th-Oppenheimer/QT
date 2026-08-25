cmake_minimum_required(VERSION 3.22)
project(spatial_vms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network OpenGLWidgets)
find_package(PahoMqttCpp QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
endif()

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
    src/EventLogTab.cpp
    src/SettingsTab.cpp
    src/SpatialMetadata.cpp
    src/SpatialProjector.cpp
    src/MqttBridge.cpp
    src/ScanCloud.cpp
    src/ScanFetcher.cpp
    src/ScanView3D.cpp
    src/EnrollDialog.cpp
    src/DemoBridge.cpp
    src/ScanListDialog.cpp
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
    src/SettingsTab.h
    src/SpatialMetadata.h
    src/SpatialProjector.h
    src/DevicesTab.h
    src/EventLogTab.h
    src/MqttBridge.h
    src/ScanCloud.h
    src/ScanFetcher.h
    src/ScanListDialog.h
    src/ScanView3D.h
    src/EnrollDialog.h
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

# resources.qrc: 사설 CA 공개 인증서를 실행파일에 박는다(발급 서버 검증용).
qt_add_executable(spatial_vms ${MACOSX_BUNDLE_FLAG} ${SOURCES} ${HEADERS}
                  resources/resources.qrc ${APP_ICON_SOURCES})

set_target_properties(spatial_vms PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "SPATIAL-VMS"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.veda4th.spatialvms"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
)

target_include_directories(spatial_vms PRIVATE src)

set(SPATIAL_VMS_LIBS Qt6::Widgets Qt6::Network Qt6::OpenGLWidgets)

if(TARGET PahoMqttCpp::paho-mqttpp3)
    target_compile_definitions(spatial_vms PRIVATE USE_PAHO_MQTT)
    list(APPEND SPATIAL_VMS_LIBS PahoMqttCpp::paho-mqttpp3)
    if(WIN32)
        target_compile_definitions(spatial_vms PRIVATE PAHO_MQTTPP_IMPORTS)
    endif()
endif()

# FFmpeg 연동 (PkgConfig 또는 Windows MinGW GStreamer/Qt libav* 라이브러리)
if(TARGET PkgConfig::FFMPEG)
    target_compile_definitions(spatial_vms PRIVATE USE_FFMPEG)
    list(APPEND SPATIAL_VMS_LIBS PkgConfig::FFMPEG)
elseif(EXISTS "C:/PROGRA~1/gstreamer/1.0/mingw_x86_64/include" AND EXISTS "C:/Qt/6.11.1/mingw_64/bin/libavcodec.dll.a")
    target_compile_definitions(spatial_vms PRIVATE USE_FFMPEG)
    target_include_directories(spatial_vms PRIVATE "C:/PROGRA~1/gstreamer/1.0/mingw_x86_64/include")
    list(APPEND SPATIAL_VMS_LIBS
        "C:/Qt/6.11.1/mingw_64/bin/libavcodec.dll.a"
        "C:/Qt/6.11.1/mingw_64/bin/libavformat.dll.a"
        "C:/Qt/6.11.1/mingw_64/bin/libavutil.dll.a"
        "C:/Qt/6.11.1/mingw_64/bin/libswscale.dll.a"
    )
endif()

target_link_libraries(spatial_vms PRIVATE ${SPATIAL_VMS_LIBS})

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(spatial_metadata_tests
        tests/spatial_metadata_tests.cpp
        src/SpatialMetadata.cpp
        src/SpatialMetadata.h
        src/SpatialProjector.cpp
        src/SpatialProjector.h
        src/Models.h
    )
    target_include_directories(spatial_metadata_tests PRIVATE src)
    target_link_libraries(spatial_metadata_tests PRIVATE Qt6::Core)
    add_test(NAME spatial_metadata_tests COMMAND spatial_metadata_tests)

    qt_add_executable(spatial_projection_tests
        tests/spatial_projection_tests.cpp
        src/SpatialMetadata.cpp
        src/SpatialMetadata.h
        src/SpatialProjector.cpp
        src/SpatialProjector.h
        src/Models.h
    )
    target_include_directories(spatial_projection_tests PRIVATE src)
    target_link_libraries(spatial_projection_tests PRIVATE Qt6::Core)
    add_test(NAME spatial_projection_tests COMMAND spatial_projection_tests)

    if(TARGET PkgConfig::FFMPEG OR (EXISTS "C:/PROGRA~1/gstreamer/1.0/mingw_x86_64/include" AND EXISTS "C:/Qt/6.11.1/mingw_64/bin/libavcodec.dll.a"))
        qt_add_executable(rtsp_metadata_source_tests
            tests/rtsp_metadata_source_tests.cpp
            src/RtspSource.cpp
            src/RtspSource.h
            src/RtspDecoder.cpp
            src/RtspDecoder.h
            src/SpatialMetadata.cpp
            src/SpatialMetadata.h
            src/SpatialProjector.cpp
            src/SpatialProjector.h
            src/Models.h
            src/ConfigPath.h
        )
        target_include_directories(rtsp_metadata_source_tests PRIVATE src)
        if(TARGET PkgConfig::FFMPEG)
            target_link_libraries(rtsp_metadata_source_tests PRIVATE Qt6::Core Qt6::Gui PkgConfig::FFMPEG)
        else()
            target_compile_definitions(rtsp_metadata_source_tests PRIVATE USE_FFMPEG)
            target_include_directories(rtsp_metadata_source_tests PRIVATE "C:/PROGRA~1/gstreamer/1.0/mingw_x86_64/include")
            target_link_libraries(rtsp_metadata_source_tests PRIVATE Qt6::Core Qt6::Gui
                "C:/Qt/6.11.1/mingw_64/bin/libavcodec.dll.a"
                "C:/Qt/6.11.1/mingw_64/bin/libavformat.dll.a"
                "C:/Qt/6.11.1/mingw_64/bin/libavutil.dll.a"
                "C:/Qt/6.11.1/mingw_64/bin/libswscale.dll.a"
            )
        endif()
        add_test(NAME rtsp_metadata_source_tests COMMAND rtsp_metadata_source_tests)
    endif()

    qt_add_executable(spatial_metadata_replay
        tools/spatial_metadata_replay.cpp
        src/SpatialMetadata.cpp
        src/SpatialMetadata.h
        src/SpatialProjector.cpp
        src/SpatialProjector.h
        src/Models.h
        src/Theme.h
        src/TopViewPanel.cpp
        src/TopViewPanel.h
        src/TopViewWidget.cpp
        src/TopViewWidget.h
        src/ScanView3D.cpp
        src/ScanView3D.h
        src/ScanCloud.cpp
        src/ScanCloud.h
    )
    target_include_directories(spatial_metadata_replay PRIVATE src)
    target_link_libraries(spatial_metadata_replay PRIVATE Qt6::Widgets Qt6::OpenGLWidgets)
endif()
