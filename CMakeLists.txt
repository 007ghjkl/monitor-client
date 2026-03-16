cmake_minimum_required(VERSION 3.16)

project(monitor_client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
# find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Multimedia)
find_package(Qt6 REQUIRED COMPONENTS StateMachine)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
#查找库文件:FFmpeg
if(WIN32)
    set(FFMPEG_ROOT "E:/Tools/ffmpeg-8.0-full_build-shared" CACHE PATH "FFmpeg 安装目录")
    find_path(FFMPEG_INCLUDE_DIR
            NAMES libavformat/avformat.h libavcodec/avcodec.h
            PATHS ${FFMPEG_ROOT}/include
            REQUIRED
    )
    find_library(FFMPEG_AVFORMAT_LIB avformat PATHS ${FFMPEG_ROOT}/lib REQUIRED)
    find_library(FFMPEG_AVCODEC_LIB avcodec PATHS ${FFMPEG_ROOT}/lib REQUIRED)
    find_library(FFMPEG_AVUTIL_LIB avutil PATHS ${FFMPEG_ROOT}/lib REQUIRED)
    find_library(FFMPEG_SWRESAMPLE_LIB swresample PATHS ${FFMPEG_ROOT}/lib REQUIRED)
    set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
    set(FFMPEG_LIBRARIES
        ${FFMPEG_AVFORMAT_LIB}
        ${FFMPEG_AVCODEC_LIB}
        ${FFMPEG_AVUTIL_LIB}
        ${FFMPEG_SWRESAMPLE_LIB}
    )
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
        pkg_check_modules(FFMPEG REQUIRED
            libavformat
            libavcodec
            libavutil
            libswresample
        )
endif()
set(PROJECT_SOURCES
        src/main.cpp
        src/MainWindow.cpp
        src/MainWindow.h
        ui/MainWindow.ui
)
include_directories(${PROJECT_SOURCE_DIR})
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(monitor_client
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        src/AVProducer.h src/AVProducer.cpp
        src/AudioConsumer.h src/AudioConsumer.cpp
        src/AVBufferPool.h src/AVBufferPool.cpp
        src/VideoDisplay.h src/VideoDisplay.cpp
        src/VideoConsumer.h src/VideoConsumer.cpp
        src/DialogConnectToUrl.h src/DialogConnectToUrl.cpp ui/DialogConnectToUrl.ui

    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET monitor_client APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(monitor_client SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(monitor_client
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# target_link_libraries(monitor_client PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(monitor_client PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(monitor_client PRIVATE Qt6::Multimedia)
target_link_libraries(monitor_client PRIVATE Qt6::StateMachine)
target_link_libraries(monitor_client PRIVATE Qt6::OpenGLWidgets)
#链接库文件:FFmpeg
target_include_directories(monitor_client PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(monitor_client PRIVATE ${FFMPEG_LIBRARIES})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.monitor_client)
endif()
set_target_properties(monitor_client PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS monitor_client
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(monitor_client)
endif()
