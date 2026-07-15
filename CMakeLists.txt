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
    FetchContent_MakeAvailable(glfw imgui implot nlohmann_json stb)

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
    target_include_directories(blackjack_gui PRIVATE ${stb_SOURCE_DIR})
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