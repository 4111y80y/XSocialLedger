cmake_minimum_required(VERSION 3.16)

project(XSocialLedger VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check compiler - WebView2 requires MSVC
if(NOT MSVC)
    message(WARNING
        "WebView2 requires MSVC. Using ${CMAKE_CXX_COMPILER_ID} may cause link errors. "
        "Recommend: Select 'Desktop Qt 6.10.1 MSVC 2022 64bit' kit in Qt Creator.")
endif()

# Qt configuration
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "D:/Qt/6.10.1/msvc2022_64")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# WebView2 SDK configuration
set(WEBVIEW2_ROOT "${CMAKE_SOURCE_DIR}/third_party/webview2")

# Source files
set(PROJECT_SOURCES
    src/main.cpp
    # App
    src/App/WebView2App.h
    src/App/WebView2App.cpp
    src/App/WebView2Handler.h
    src/App/WebView2Handler.cpp
    # UI
    src/UI/MainWindow.h
    src/UI/MainWindow.cpp
    src/UI/WebView2Widget.h
    src/UI/WebView2Widget.cpp
    src/UI/ActionListPanel.h
    src/UI/ActionListPanel.cpp
    src/UI/StatsPanel.h
    src/UI/StatsPanel.cpp
    # Data
    src/Data/SocialAction.h
    src/Data/DataStorage.h
    src/Data/DataStorage.cpp
    # Core
    src/Core/NotificationCollector.h
    src/Core/NotificationCollector.cpp
    src/Core/ReciprocatorEngine.h
    src/Core/ReciprocatorEngine.cpp
)

# Create executable
qt_add_executable(XSocialLedger
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

# Include directories
target_include_directories(XSocialLedger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    "${WEBVIEW2_ROOT}/build/native/include"
)

# Compile definitions for WebView2
target_compile_definitions(XSocialLedger PRIVATE
    WIN32
    _WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    NOMINMAX
)

# Link Qt libraries
target_link_libraries(XSocialLedger PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# WebView2 static library
target_link_libraries(XSocialLedger PRIVATE
    "${WEBVIEW2_ROOT}/build/native/x64/WebView2LoaderStatic.lib"
)

# Windows system libraries
target_link_libraries(XSocialLedger PRIVATE
    comctl32
    rpcrt4
    shlwapi
    ws2_32
    ole32
    oleaut32
    uuid
)

# Linker flags
if(MSVC)
    # Use UTF-8 source and execution charset
    target_compile_options(XSocialLedger PRIVATE /utf-8)
endif()

# Windows specific settings
set_target_properties(XSocialLedger PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Run windeployqt to copy Qt DLLs
if(WIN32)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET XSocialLedger POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                $<TARGET_FILE:XSocialLedger>
            COMMENT "Running windeployqt to deploy Qt DLLs..."
        )
    endif()
endif()

qt_finalize_executable(XSocialLedger)
