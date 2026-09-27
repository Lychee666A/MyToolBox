cmake_minimum_required(VERSION 3.19)
project(MyToolBox VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ============================================================
#        程序元数据（唯一数据源 —— 改这里就够了）
# ============================================================
set(MY_APP_NAME           "MyToolBox")
set(MY_APP_DISPLAY_NAME   "多功能轻量工具箱")

# ★ 语义化版本主体（手动维护，发新版本时改这里）
set(MY_APP_VERSION_CORE   "1.0.7")

set(MY_APP_COMPANY        "Lychee666")
set(MY_APP_DESCRIPTION    "集成拍照、录像、录屏、播放、压缩包预览、图片查看、文件搜索、内嵌浏览器的轻量工具箱")
set(MY_APP_COPYRIGHT      "Copyright (C) 2025 Lychee666. All rights reserved.")
set(MY_APP_ORIGINAL_FILE  "${MY_APP_NAME}.exe")

# Helper 专用元数据
set(MY_HELPER_NAME        "MyToolBoxBrowser")
set(MY_HELPER_DISPLAY     "MyToolBox 浏览器")
set(MY_HELPER_DESCRIPTION "MyToolBox 独立浏览器（WebView2）")

# ============================================================
#        ★ 版本号自动生成
#        格式：<core>-0.<N>.0+<git-hash>
#          - core       = MY_APP_VERSION_CORE（1.0.0）
#          - N          = version.txt 里的构建号（每次 cmake configure +1）
#          - git-hash   = git rev-parse --short=9 HEAD
# ============================================================

# ---- 1. 读取/递增构建号 ----
set(VERSION_FILE "${CMAKE_SOURCE_DIR}/version.txt")

if(EXISTS "${VERSION_FILE}")
    file(READ "${VERSION_FILE}" VER_BUILD)
    string(STRIP "${VER_BUILD}" VER_BUILD)
    if(NOT VER_BUILD MATCHES "^[0-9]+$")
        set(VER_BUILD 0)
    endif()
else()
    set(VER_BUILD 0)
endif()

# 每次 cmake configure 时 +1（构建号只增不减）
math(EXPR VER_BUILD "${VER_BUILD} + 1")
file(WRITE "${VERSION_FILE}" "${VER_BUILD}")

# ---- 2. 抓 Git 短 hash ----
find_package(Git QUIET)
set(GIT_HASH "nogit")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=9 HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_HASH STREQUAL "")
        set(GIT_HASH "nogit")
    endif()
endif()

# ---- 3. 拼完整版本号 ----
set(MY_APP_VERSION "${MY_APP_VERSION_CORE}-0.${VER_BUILD}.0+${GIT_HASH}")

# ---- 4. 从 CORE 拆出数字版本（供 .rc 的 FILEVERSION 使用）----
string(REPLACE "." ";" _ver_list "${MY_APP_VERSION_CORE}")
list(LENGTH _ver_list _ver_count)
list(GET _ver_list 0 VER_MAJOR)
if(_ver_count GREATER 1)
    list(GET _ver_list 1 VER_MINOR)
else()
    set(VER_MINOR 0)
endif()
if(_ver_count GREATER 2)
    list(GET _ver_list 2 VER_PATCH)
else()
    set(VER_PATCH 0)
endif()
# VER_BUILD 已在前面从 version.txt 读出并 +1，直接用作 .rc 的第 4 段

message(STATUS "============================================================")
message(STATUS "  Version : ${MY_APP_VERSION}")
message(STATUS "  Core    : ${MY_APP_VERSION_CORE}")
message(STATUS "  Build   : ${VER_BUILD}")
message(STATUS "  GitHash : ${GIT_HASH}")
message(STATUS "  RC ver  : ${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}.${VER_BUILD}")
message(STATUS "============================================================")

# ============================================================
#        强制 Qt / RC 编译器使用 MSVC 版本
# ============================================================
if(MSVC)
    set(CMAKE_PREFIX_PATH "F:/Qt/6.11.2/msvc2022_64"
        CACHE PATH "Qt MSVC path" FORCE)

    if(DEFINED CMAKE_VS_WINDOWS_SDK_BIN_PATH)
        set(_rc_candidate "${CMAKE_VS_WINDOWS_SDK_BIN_PATH}/x64/rc.exe")
    else()
        set(_rc_candidate "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/rc.exe")
    endif()

    if(EXISTS "${_rc_candidate}")
        set(CMAKE_RC_COMPILER "${_rc_candidate}"
            CACHE FILEPATH "MSVC RC compiler" FORCE)
        message(STATUS "=== CMAKE_RC_COMPILER = ${CMAKE_RC_COMPILER} ===")
    else()
        message(WARNING "找不到 MSVC 的 rc.exe，候选路径: ${_rc_candidate}")
    endif()
endif()

# ============================================================
#        Qt
# ============================================================
find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Multimedia MultimediaWidgets Gui Concurrent
)

set(MY_APP_COMMENTS "Built with Qt ${Qt6_VERSION}")

# ============================================================
#        路径定义
# ============================================================
set(SRC_DIR        ${CMAKE_SOURCE_DIR}/src)
set(HELPER_DIR     ${CMAKE_SOURCE_DIR}/helper)
set(BIT7Z_DIR      ${CMAKE_SOURCE_DIR}/third_party/bit7z)
set(VLC_DIR        ${CMAKE_SOURCE_DIR}/third_party/vlc)
set(EVERYTHING_DIR ${CMAKE_SOURCE_DIR}/third_party/everything)
set(WEBVIEW2_DIR   ${CMAKE_SOURCE_DIR}/third_party/WebView2)

# ============================================================
#        生成 .rc（主程序 + Helper，各自独立模板）
# ============================================================
set(ICON_PATH "${CMAKE_SOURCE_DIR}/resources/icon.ico")
file(TO_CMAKE_PATH "${ICON_PATH}" ICON_PATH_FWD)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

# ---- 主程序 .rc ----
configure_file(
    "${CMAKE_SOURCE_DIR}/resources/version.rc.in"
    "${CMAKE_BINARY_DIR}/generated/app.rc"
    @ONLY
)

# ---- Helper .rc（独立模板）----
configure_file(
    "${CMAKE_SOURCE_DIR}/resources/version_helper.rc.in"
    "${CMAKE_BINARY_DIR}/generated/helper.rc"
    @ONLY
)

message(STATUS "=== Generated .rc: ${CMAKE_BINARY_DIR}/generated/app.rc ===")
message(STATUS "=== Generated .rc: ${CMAKE_BINARY_DIR}/generated/helper.rc ===")

# ============================================================
#        共享源文件列表（主程序和 Helper 共用）
# ============================================================
set(SHARED_BROWSER_SOURCES
    # ---- 浏览器核心 ----
    ${SRC_DIR}/WebBrowserWidget.cpp
    ${SRC_DIR}/WebBrowserWidget.h
    ${SRC_DIR}/WebPageView.cpp
    ${SRC_DIR}/WebPageView.h
    ${SRC_DIR}/WebView2Runtime.cpp
    ${SRC_DIR}/WebView2Runtime.h

    # ---- 历史 / 下载 ----
    ${SRC_DIR}/BrowserHistory.cpp
    ${SRC_DIR}/BrowserHistory.h
    ${SRC_DIR}/DownloadsManager.cpp
    ${SRC_DIR}/DownloadsManager.h
    ${SRC_DIR}/HistoryDialog.cpp
    ${SRC_DIR}/HistoryDialog.h
    ${SRC_DIR}/DownloadsDialog.cpp
    ${SRC_DIR}/DownloadsDialog.h

    # ---- 设置 / 主题 ----
    ${SRC_DIR}/SettingsManager.cpp
    ${SRC_DIR}/SettingsManager.h
    ${SRC_DIR}/ThemeManager.cpp
    ${SRC_DIR}/ThemeManager.h
    ${SRC_DIR}/SearchEngineDialog.cpp
    ${SRC_DIR}/SearchEngineDialog.h
    ${SRC_DIR}/SettingsDialog.cpp
    ${SRC_DIR}/SettingsDialog.h
    ${SRC_DIR}/AppInfo.h
)

# ============================================================
#        主程序：MyToolBox
# ============================================================
add_executable(MyToolBox WIN32
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/MainWindow.cpp
    ${SRC_DIR}/CameraRecorder.cpp
    ${SRC_DIR}/ScreenRecorderWidget.cpp
    ${SRC_DIR}/WasapiLoopbackCapture.cpp
    ${SRC_DIR}/VlcPlayerWidget.cpp
    ${SRC_DIR}/ArchivePreviewWidget.cpp
    ${SRC_DIR}/ImageViewerWidget.cpp
    ${SRC_DIR}/QuickLinksWidget.cpp
    ${SRC_DIR}/SettingsDialog.cpp
    ${SRC_DIR}/SettingsManager.cpp
    ${SRC_DIR}/FileSearchWidget.cpp
    ${SRC_DIR}/EverythingApi.cpp
    ${SRC_DIR}/WebBrowserWidget.cpp
    ${SRC_DIR}/WebView2Runtime.cpp
    ${SRC_DIR}/WebPageView.cpp
    ${SRC_DIR}/SearchEngineDialog.cpp
    ${SRC_DIR}/AboutDialog.cpp
    ${SRC_DIR}/ThemeManager.cpp

    # ---- 历史 / 下载 ----
    ${SRC_DIR}/BrowserHistory.cpp
    ${SRC_DIR}/DownloadsManager.cpp
    ${SRC_DIR}/HistoryDialog.cpp
    ${SRC_DIR}/DownloadsDialog.cpp

    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
    ${CMAKE_BINARY_DIR}/generated/app.rc
)

target_compile_definitions(MyToolBox PRIVATE
    MYTOOLBOX_VERSION="${MY_APP_VERSION}"
    MYTOOLBOX_DISPLAY_NAME="${MY_APP_DISPLAY_NAME}"
)

if(MSVC)
    target_compile_options(MyToolBox PRIVATE
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )
endif()

target_include_directories(MyToolBox PRIVATE
    ${SRC_DIR}
    ${BIT7Z_DIR}/include
    ${VLC_DIR}/include
    ${EVERYTHING_DIR}/include
    ${WEBVIEW2_DIR}/include
)

# ---- bit7z ----
find_library(BIT7Z_LIB_DEBUG
    NAMES bit7z bit7zd
    PATHS ${BIT7Z_DIR}/lib/x64/Debug
    NO_DEFAULT_PATH
)
find_library(BIT7Z_LIB_RELEASE
    NAMES bit7z
    PATHS ${BIT7Z_DIR}/lib/x64/Release
    NO_DEFAULT_PATH
)
if(NOT BIT7Z_LIB_RELEASE)
    message(FATAL_ERROR "找不到 bit7z Release 库: ${BIT7Z_DIR}/lib/x64/Release")
endif()
if(NOT BIT7Z_LIB_DEBUG)
    message(WARNING "找不到 bit7z Debug 库: ${BIT7Z_DIR}/lib/x64/Debug")
endif()
set(BIT7Z_LIB
    $<$<CONFIG:Debug>:${BIT7Z_LIB_DEBUG}>
    $<$<CONFIG:Release>:${BIT7Z_LIB_RELEASE}>
)

# ---- VLC ----
find_library(VLC_LIB NAMES libvlc vlc
    PATHS ${VLC_DIR}/lib NO_DEFAULT_PATH)
find_library(VLCCORE_LIB NAMES libvlccore vlccore
    PATHS ${VLC_DIR}/lib NO_DEFAULT_PATH)
if(NOT VLC_LIB OR NOT VLCCORE_LIB)
    message(FATAL_ERROR "找不到 VLC 库")
endif()

# ---- WebView2 ----
find_library(WEBVIEW2_LOADER_LIB
    NAMES WebView2LoaderStatic
    PATHS ${WEBVIEW2_DIR}/lib/x64
    NO_DEFAULT_PATH
)
if(NOT WEBVIEW2_LOADER_LIB)
    message(FATAL_ERROR "找不到 WebView2LoaderStatic.lib: ${WEBVIEW2_DIR}/lib/x64")
endif()
message(STATUS "=== WEBVIEW2_LOADER_LIB = ${WEBVIEW2_LOADER_LIB} ===")

target_link_libraries(MyToolBox PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Concurrent

    ${BIT7Z_LIB}
    ${VLC_LIB}
    ${VLCCORE_LIB}
    ${WEBVIEW2_LOADER_LIB}

    ole32
    oleaut32
    avrt
)

# ---- 主程序运行时拷贝 ----
add_custom_command(TARGET MyToolBox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/7z.dll"
        "$<TARGET_FILE_DIR:MyToolBox>"
    COMMENT "Copying 7z.dll"
)

add_custom_command(TARGET MyToolBox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${VLC_DIR}/bin"
        "$<TARGET_FILE_DIR:MyToolBox>"
    COMMENT "Copying VLC runtime"
)

add_custom_command(TARGET MyToolBox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EVERYTHING_DIR}/bin/Everything.exe"
        "$<TARGET_FILE_DIR:MyToolBox>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EVERYTHING_DIR}/bin/Everything.lng"
        "$<TARGET_FILE_DIR:MyToolBox>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EVERYTHING_DIR}/bin/Everything64.dll"
        "$<TARGET_FILE_DIR:MyToolBox>"
    COMMENT "Copying Everything runtime"
)

add_custom_command(TARGET MyToolBox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBVIEW2_DIR}/bin/WebView2Loader.dll"
        "$<TARGET_FILE_DIR:MyToolBox>"
    COMMENT "Copying WebView2Loader.dll"
)

add_custom_command(TARGET MyToolBox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBVIEW2_DIR}/bin/MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
        "$<TARGET_FILE_DIR:MyToolBox>"
    COMMENT "Copying WebView2 Runtime installer"
)

# ============================================================
#        Helper：MyToolBoxBrowser
# ============================================================
add_executable(MyToolBoxBrowser WIN32
    ${HELPER_DIR}/main.cpp
    ${SHARED_BROWSER_SOURCES}
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
    ${CMAKE_BINARY_DIR}/generated/helper.rc
)

target_compile_definitions(MyToolBoxBrowser PRIVATE
    MYTOOLBOX_VERSION="${MY_APP_VERSION}"
    MYTOOLBOX_DISPLAY_NAME="${MY_HELPER_DISPLAY}"
)

if(MSVC)
    target_compile_options(MyToolBoxBrowser PRIVATE
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )
endif()

target_include_directories(MyToolBoxBrowser PRIVATE
    ${SRC_DIR}
    ${WEBVIEW2_DIR}/include
)

target_link_libraries(MyToolBoxBrowser PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui

    ${WEBVIEW2_LOADER_LIB}

    ole32
    oleaut32
)

# ---- Helper 运行时拷贝 ----
add_custom_command(TARGET MyToolBoxBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBVIEW2_DIR}/bin/WebView2Loader.dll"
        "$<TARGET_FILE_DIR:MyToolBoxBrowser>"
    COMMENT "Copying WebView2Loader.dll for helper"
)

add_custom_command(TARGET MyToolBoxBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBVIEW2_DIR}/bin/MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
        "$<TARGET_FILE_DIR:MyToolBoxBrowser>"
    COMMENT "Copying WebView2 Runtime installer for helper"
)

# 让两个 exe 输出到同一个目录，共享 Qt DLL
set_target_properties(MyToolBoxBrowser PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "$<TARGET_FILE_DIR:MyToolBox>"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "$<TARGET_FILE_DIR:MyToolBox>"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "$<TARGET_FILE_DIR:MyToolBox>"
)

# ============================================================
#        windeployqt（两个 exe 都做）
# ============================================================
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${QT_DIR}/bin" "$ENV{QT_DIR}/bin"
        PATHS "${Qt6_DIR}/../../../bin"
        NO_DEFAULT_PATH
    )
    if(NOT WINDEPLOYQT_EXECUTABLE)
        get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION_RELEASE)
        if(_qt_core_loc)
            get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)
            find_program(WINDEPLOYQT_EXECUTABLE
                NAMES windeployqt
                HINTS "${_qt_bin_dir}"
                NO_DEFAULT_PATH)
        endif()
    endif()
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET MyToolBox POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:MyToolBox>"
            COMMENT "Running windeployqt for MyToolBox"
        )
        add_custom_command(TARGET MyToolBoxBrowser POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:MyToolBoxBrowser>"
            COMMENT "Running windeployqt for MyToolBoxBrowser"
        )
    else()
        message(WARNING "windeployqt 未找到")
    endif()
endif()