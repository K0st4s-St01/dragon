cmake_minimum_required(VERSION 3.10)
project(dragon_editor LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(GL   REQUIRED gl)

add_library(glad STATIC vendor/glad/src/glad.c)
target_include_directories(glad PUBLIC vendor/glad/include)

add_library(tomlc99 STATIC vendor/tomlc99/toml.c)
target_include_directories(tomlc99 PUBLIC vendor/tomlc99)

set(EDITOR_SOURCES
    src/main.c
    src/core/app.c
    src/core/renderer.c
    src/core/mode.c
    src/core/command.c
    src/core/input.c
    src/editor/config.c
    src/editor/cursor.c
    src/editor/buffer.c
    src/editor/document.c
    src/editor/history.c
    src/editor/lsp.c
    src/editor/lsp_config.c
    src/editor/syntax.c
    src/editor/treesitter.c
    src/gui/gui.c
    src/gui/text.c
    src/gui/theme.c
    src/gui/panels/statusbar.c
    src/gui/panels/file_browser.c
    src/gui/panels/find_replace.c
    src/gui/panels/goto_line.c
    src/gui/panels/about.c
    src/gui/panels/buffer_picker.c
    src/gui/panels/jumplist_picker.c
    src/gui/panels/lsp_goto_picker.c
    src/gui/panels/lsp_hover_panel.c
    src/gui/panels/lsp_diagnostics_picker.c
    src/gui/panels/space_menu.c
    src/gui/panels/symbols_picker.c
    src/gui/panels/rename.c
    src/gui/panels/code_actions.c
)

add_executable(${PROJECT_NAME} ${EDITOR_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/dragon_editor
    ${GLFW3_INCLUDE_DIRS}
    ${GL_INCLUDE_DIRS}
    vendor/glad/include
    vendor/stb
    /usr/include/tree_sitter
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glad
    tomlc99
    ${GLFW3_LIBRARIES}
    ${GL_LIBRARIES}
    m
    dl
    tree-sitter
)

target_compile_options(${PROJECT_NAME} PRIVATE
    ${GLFW3_CFLAGS_OTHER}
    -Wall -Wextra
)
