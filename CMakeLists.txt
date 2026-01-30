cmake_minimum_required(VERSION 3.16)

project(chatClient LANGUAGES CXX)

# C++ 标准设置。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成 compile_commands.json，供 clangd 等工具使用。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 开启 Qt 的自动 MOC/UIC/RCC 处理。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 同时支持 Qt6 与 Qt5。
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# 应用目标（收集 src 目录下所有文件）。
file(GLOB_RECURSE CHATCLIENT_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*"
)
add_executable(chatClient ${CHATCLIENT_SOURCES})

# 头文件搜索路径：包含 src 及其所有子目录，便于直接 include 头文件。
set(CHATCLIENT_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src")
foreach(source_file IN LISTS CHATCLIENT_SOURCES)
    get_filename_component(source_dir "${source_file}" DIRECTORY)
    list(APPEND CHATCLIENT_INCLUDE_DIRS "${source_dir}")
endforeach()
list(REMOVE_DUPLICATES CHATCLIENT_INCLUDE_DIRS)
target_include_directories(chatClient PRIVATE ${CHATCLIENT_INCLUDE_DIRS})

# 链接 Qt Widgets 模块。
target_link_libraries(chatClient PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Windows 下隐藏控制台窗口（GUI 应用）。
if (WIN32)
    set_target_properties(chatClient PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
