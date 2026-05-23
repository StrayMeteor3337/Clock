cmake_minimum_required(VERSION 3.20)

project(FocusClock LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}")
endforeach()

add_executable(FocusClock WIN32
    src/AppCore.cpp
    src/Layout.cpp
    src/main.cpp
    src/Panel.cpp
    src/Rendering.cpp
    src/Res/FocusClock.rc
    src/Rerun.cpp
    src/Schedule.cpp
    src/Settings.cpp
    src/Update.cpp
    src/Utilities.cpp
    src/Whitelist.cpp
)

if(MINGW)
    target_link_options(FocusClock PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    target_compile_options(FocusClock PRIVATE /utf-8)
endif()

target_compile_definitions(FocusClock PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(FocusClock PRIVATE
    comdlg32
    gdiplus
    dwmapi
    ole32
    shell32
    winhttp
)

add_custom_target(CopyWhitelist ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Whitelist.txt"
        "$<TARGET_FILE_DIR:FocusClock>/Whitelist.txt"
    DEPENDS FocusClock "${CMAKE_CURRENT_SOURCE_DIR}/Whitelist.txt"
)
