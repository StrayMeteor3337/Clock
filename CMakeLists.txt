cmake_minimum_required(VERSION 3.20)

project(FocusClock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FocusClock WIN32
    src/main.cpp
    src/FocusTimerCore.cpp
    src/WhitelistManager.cpp
    src/ScheduleManager.cpp
    src/WindowManager.cpp
    src/KeyboardHook.cpp
    src/SettingsManager.cpp
    src/UIManager.cpp
    src/utils.cpp
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
    NOMINMAX
)

target_link_libraries(FocusClock PRIVATE
    comdlg32
    gdiplus
    dwmapi
    ole32
    shell32
)

add_custom_target(CopyWhitelist ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Whitelist.txt"
        "$<TARGET_FILE_DIR:FocusClock>/Whitelist.txt"
    DEPENDS FocusClock "${CMAKE_CURRENT_SOURCE_DIR}/Whitelist.txt"
)
