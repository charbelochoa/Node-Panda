cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(NodePanda LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.1-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

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

FetchContent_Declare(
    lua_src
    URL      https://www.lua.org/ftp/lua-5.4.6.tar.gz
    URL_HASH SHA256=7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua_src)

file(GLOB LUA_SRC "${lua_src_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM LUA_SRC "${lua_src_SOURCE_DIR}/src/lua.c")
list(REMOVE_ITEM LUA_SRC "${lua_src_SOURCE_DIR}/src/luac.c")

add_library(lua_lib STATIC ${LUA_SRC})
target_include_directories(lua_lib PUBLIC "${lua_src_SOURCE_DIR}/src")
if(WIN32)
    target_compile_definitions(lua_lib PRIVATE LUA_USE_WINDOWS)
endif()
if(MSVC)
    target_compile_options(lua_lib PRIVATE /W0)
endif()

FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        v3.3.0
    GIT_SHALLOW    TRUE
)
set(SOL2_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sol2)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

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
    src/plugin_manager.cpp
    src/plugin_panel.cpp
    src/query_engine.cpp
    src/query_panel.cpp
    src/graph_analytics.cpp
    src/analytics_panel.cpp
    src/srs_engine.cpp
    src/srs_panel.cpp
    src/git_manager.cpp
    src/git_panel.cpp
    src/http_server.cpp
    src/http_panel.cpp
    src/crypto_engine.cpp
    src/auth_manager.cpp
    src/database_manager.cpp
    src/database_panel.cpp
    src/ui_theme.cpp
    src/ui_components.cpp
    src/outliner_engine.cpp
    src/outliner_panel.cpp
    src/task_engine.cpp
    src/task_panel.cpp
    src/journal_engine.cpp
    src/journal_panel.cpp
    src/whiteboard_engine.cpp
    src/whiteboard_panel.cpp
    src/datalog_engine.cpp
    src/property_engine.cpp
    src/asset_manager.cpp
    src/asset_panel.cpp
    src/backup_manager.cpp
    src/sync_engine.cpp
    src/sync_panel.cpp
    src/command_palette.cpp
    src/ai_catalog.cpp
    src/ai_provider.cpp
    src/context_brain.cpp
    src/skill_manager.cpp
    src/usage_ledger.cpp
    src/data_lab.cpp
    src/evolution_engine.cpp
    src/agent_engine.cpp
    src/agent_panel.cpp
    src/lab_panel.cpp
    src/cli_runner.cpp
    src/pdf_engine.cpp
    src/pdf_panel.cpp
    src/websocket_rtc.cpp
    src/rtc_panel.cpp
    src/table_view_engine.cpp
    src/table_view_panel.cpp
    src/vector_engine.cpp
    src/vector_panel.cpp
    src/math_renderer.cpp
    src/math_panel.cpp
    src/mobile_bridge.cpp
)

if(WIN32)
    list(APPEND SOURCES resources/app.rc)
endif()

add_executable(NodePanda ${SOURCES})

target_include_directories(NodePanda PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(NodePanda PRIVATE
    imgui_lib
    OpenGL::GL
    Threads::Threads
    lua_lib
    sol2
)

if(WIN32)
    target_link_libraries(NodePanda PRIVATE comdlg32 psapi ws2_32 winhttp)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)
    target_compile_options(NodePanda PRIVATE -Wno-template-body)
endif()

add_custom_command(TARGET NodePanda POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NodePanda>/data/notas"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NodePanda>/data/plugins"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NodePanda>/data/skills"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NodePanda>/data/sql"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/data/panda.png")
    add_custom_command(TARGET NodePanda POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/data/panda.png"
            "$<TARGET_FILE_DIR:NodePanda>/data/panda.png"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/data/plugins")
    add_custom_command(TARGET NodePanda POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/data/plugins"
            "$<TARGET_FILE_DIR:NodePanda>/data/plugins"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/data/skills")
    add_custom_command(TARGET NodePanda POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/data/skills"
            "$<TARGET_FILE_DIR:NodePanda>/data/skills"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/data/sql")
    add_custom_command(TARGET NodePanda POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/data/sql"
            "$<TARGET_FILE_DIR:NodePanda>/data/sql"
    )
endif()
