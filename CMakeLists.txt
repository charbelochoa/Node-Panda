cmake_minimum_required(VERSION 3.14)
project(NodePanda LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suprimir warnings de strncpy en MSVC
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

# ==============================================================================
# Dependencias externas via FetchContent
# ==============================================================================
include(FetchContent)

# --- GLFW (windowing) ---
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# --- Dear ImGui (UI con docking) ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.1-docking
)
FetchContent_MakeAvailable(imgui)

# Crear libreria de ImGui con backends GLFW + OpenGL3
set(IMGUI_DIR ${imgui_SOURCE_DIR})
add_library(imgui_lib STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# --- Lua 5.4.6 (compilado directamente desde fuente oficial) ---
FetchContent_Declare(
    lua_src
    URL      https://www.lua.org/ftp/lua-5.4.6.tar.gz
    URL_HASH SHA256=7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua_src)

# Compilar nucleo de Lua como libreria estatica
# (excluir lua.c y luac.c — son los ejecutables standalone, no el nucleo)
file(GLOB LUA_SRC "${lua_src_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM LUA_SRC "${lua_src_SOURCE_DIR}/src/lua.c")
list(REMOVE_ITEM LUA_SRC "${lua_src_SOURCE_DIR}/src/luac.c")

add_library(lua_lib STATIC ${LUA_SRC})
target_include_directories(lua_lib PUBLIC "${lua_src_SOURCE_DIR}/src")
if(WIN32)
    target_compile_definitions(lua_lib PRIVATE LUA_USE_WINDOWS)
endif()
# Suprimir warnings de MSVC en codigo C de Lua
if(MSVC)
    target_compile_options(lua_lib PRIVATE /W0)
endif()

# --- sol2 v3.3.0 (bindings C++ <-> Lua, header-only) ---
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        v3.3.0
)
set(SOL2_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sol2)

# --- OpenGL ---
find_package(OpenGL REQUIRED)

# ==============================================================================
# Proyecto NodePanda
# ==============================================================================
set(SOURCES
    src/main.cpp
    src/note.cpp
    src/note_manager.cpp
    src/markdown_parser.cpp
    src/graph.cpp
    src/force_layout.cpp
    src/app.cpp
    src/editor_panel.cpp
    src/explorer_panel.cpp
    src/graph_panel.cpp
    src/frontmatter.cpp
    src/backlinks_panel.cpp
    src/image_cache.cpp
    src/file_watcher.cpp
    src/quadtree.cpp
    src/texture_manager.cpp
    src/ai_exporter.cpp
    src/performance_metrics.cpp
    src/lua_manager.cpp
    src/lua_console.cpp
    src/memory_engine.cpp
    src/memory_panel.cpp
    src/command_panel.cpp
)

# Icono y recursos de Windows
if(WIN32)
    list(APPEND SOURCES resources/app.rc)
    # WIN32 = subsistema Windows (sin ventana de consola negra al abrir el .exe)
    add_executable(NodePanda WIN32 ${SOURCES})
    # El proyecto usa int main() — indicar a MSVC que use mainCRTStartup como entry point
    if(MSVC)
        set_target_properties(NodePanda PROPERTIES
            LINK_FLAGS "/ENTRY:mainCRTStartup"
        )
    endif()
else()
    add_executable(NodePanda ${SOURCES})
endif()

target_include_directories(NodePanda PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(NodePanda PRIVATE
    imgui_lib
    OpenGL::GL
    lua_lib
    sol2
)

# Vincular comdlg32 para el diálogo nativo de archivos (GetOpenFileNameA)
if(WIN32)
    target_link_libraries(NodePanda PRIVATE comdlg32 psapi)
endif()

# Copiar la carpeta de datos al directorio de salida
add_custom_command(TARGET NodePanda POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NodePanda>/data/notas"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/data/panda.png"
        "$<TARGET_FILE_DIR:NodePanda>/data/panda.png"
    COMMENT "Copiando assets de datos al directorio de salida"
)