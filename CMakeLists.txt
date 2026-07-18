cmake_minimum_required(VERSION 3.10)
project(blackjack)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BLACKJACK_GUI "Build the ImGui/ImPlot GUI (blackjack_gui)" ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG")
elseif (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
endif ()

# Link-time optimization: the hot loop in simulation.cpp calls getTrueCount and
# betFromTrueCount from blackjack.cpp, so without LTO those stay out-of-line.
# CMP0069 must be NEW or INTERPROCEDURAL_OPTIMIZATION is silently ignored here.
if (POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif ()
include(CheckIPOSupported)
check_ipo_supported(RESULT BLACKJACK_IPO_SUPPORTED OUTPUT BLACKJACK_IPO_ERROR)
if (BLACKJACK_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else ()
    message(STATUS "LTO not supported: ${BLACKJACK_IPO_ERROR}")
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(blackjack_core
        src/config.cpp
        src/print.cpp
        src/model.cpp
        src/actions.cpp
        src/blackjack.cpp
        src/simulation.cpp
        src/cli.cpp
)

target_include_directories(blackjack_core PUBLIC src)

add_executable(blackjack src/main.cpp)
target_link_libraries(blackjack PRIVATE blackjack_core)

if (BLACKJACK_GUI)
    include(FetchContent)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
            glfw
            URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(
            imgui
            URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.9.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(
            implot
            URL https://github.com/epezent/implot/archive/refs/tags/v0.16.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(
            nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(
            stb
            URL https://github.com/nothings/stb/archive/5736b15f7ea0ffb08dd38af21067c314d6a3aae9.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # Header-only. Point SOURCE_SUBDIR at a non-existent dir so MakeAvailable
    # populates the sources but skips pfd's own CMakeLists (its ancient
    # cmake_minimum_required is rejected by CMake >= 4).
    FetchContent_Declare(
            portable_file_dialogs
            URL https://github.com/samhocevar/portable-file-dialogs/archive/refs/tags/0.1.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            SOURCE_SUBDIR do-not-build
    )
    FetchContent_MakeAvailable(glfw imgui implot nlohmann_json stb
            portable_file_dialogs)

    find_package(OpenGL REQUIRED)

    add_library(imgui_lib STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
            ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
            ${implot_SOURCE_DIR}/implot.cpp
            ${implot_SOURCE_DIR}/implot_items.cpp
    )
    target_include_directories(imgui_lib PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
            ${implot_SOURCE_DIR}
    )
    target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

    add_executable(blackjack_gui src/gui/main_gui.cpp)
    target_include_directories(blackjack_gui PRIVATE ${stb_SOURCE_DIR}
            ${portable_file_dialogs_SOURCE_DIR})
    target_compile_definitions(blackjack_gui PRIVATE
            BJ_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
            $<$<BOOL:${WIN32}>:NOMINMAX>)
    target_link_libraries(blackjack_gui PRIVATE blackjack_core imgui_lib
            nlohmann_json::nlohmann_json)
    if (WIN32)
        target_link_libraries(blackjack_gui PRIVATE gdi32)
    endif ()
endif ()

include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(blackjack_tests
        tests/test_detect_blackjack.cpp
        tests/test_resolve_hand.cpp
        tests/test_card_counting.cpp
        tests/test_monitor.cpp
)
target_link_libraries(blackjack_tests PRIVATE blackjack_core GTest::gtest_main)
target_compile_features(blackjack_tests PRIVATE cxx_std_17)

if (TARGET gtest)
    set_target_properties(gtest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
endif ()
if (TARGET gtest_main)
    set_target_properties(gtest_main PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
endif ()

include(GoogleTest)
gtest_discover_tests(blackjack_tests)