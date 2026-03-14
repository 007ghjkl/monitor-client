cmake_minimum_required(VERSION 3.16)

project(monitor_client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Multimedia)
find_package(Qt6 REQUIRED COMPONENTS StateMachine)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
#查找库文件:FFmpeg、OpenSSL
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

    set(OPENSSL_ROOT_DIR "E:/Tools/OpenSSL-Win64" CACHE PATH "Path to OpenSSL root directory")
    find_path(OPENSSL_INCLUDE_DIR
        NAMES openssl/ssl.h
        PATHS "${OPENSSL_ROOT_DIR}/include"
        REQUIRED
    )
    find_library(OPENSSL_SSL_LIB NAMES libssl PATHS "${OPENSSL_ROOT_DIR}/lib/VC/x64/MD" REQUIRED)
    find_library(OPENSSL_CRYPTO_LIB NAMES libcrypto PATHS "${OPENSSL_ROOT_DIR}/lib/VC/x64/MD" REQUIRED)
    set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
    set(OPENSSL_LIBRARIES
        ${OPENSSL_SSL_LIB}
        ${OPENSSL_CRYPTO_LIB}
    )
endif()
set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        ui/mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(monitor_client
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        src/tavproducer.h src/tavproducer.cpp
        src/taudioconsumer.h src/taudioconsumer.cpp

        src/tavbufferpool.h src/tavbufferpool.cpp
        src/tvideodisplay.h src/tvideodisplay.cpp
        src/tvideoconsumer.h src/tvideoconsumer.cpp
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

target_link_libraries(monitor_client PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(monitor_client PRIVATE Qt6::Multimedia)
target_link_libraries(monitor_client PRIVATE Qt6::StateMachine)
target_link_libraries(monitor_client PRIVATE Qt6::OpenGLWidgets)
#链接库文件:FFmpeg、OpenSSL
if(WIN32)
    target_include_directories(monitor_client PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(monitor_client PRIVATE ${FFMPEG_LIBRARIES})
    target_include_directories(monitor_client PRIVATE ${OPENSSL_INCLUDE_DIRS})
    target_link_libraries(monitor_client PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(monitor_client PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(monitor_client PRIVATE Qt${QT_VERSION_MAJOR}::Core)
endif()
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
