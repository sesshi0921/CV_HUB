cmake_minimum_required(VERSION 3.24)

if(APPLE)
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(HOMEBREW_PREFIX)
        list(PREPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
    endif()
endif()

project(CV_HUB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CVHUB_BUILD_PLUGIN_OPENCV "Build the OpenCV plugin" ON)
option(CVHUB_BUILD_PLUGIN_PYTORCH "Build the PyTorch plugin" ON)
option(CVHUB_BUILD_GUI "Build the Dear ImGui/imnodes graph editor" ON)
option(CVHUB_BUILD_TESTS "Build CV_HUB tests" ON)
option(CVHUB_FETCH_GUI_DEPS "Fetch Dear ImGui when no CMake package is available" ON)

find_package(Threads REQUIRED)
find_package(spdlog CONFIG QUIET)
if(NOT TARGET spdlog::spdlog)
    include(FetchContent)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

add_library(cvhub_core INTERFACE)
target_include_directories(cvhub_core INTERFACE include)
target_compile_features(cvhub_core INTERFACE cxx_std_20)

set(CVHUB_RUNTIME_SOURCES
    src/runtime/function_node_generator.cpp
    src/runtime/node_catalog.cpp
    src/runtime/pipeline_executor.cpp
    src/runtime/service_container.cpp
    src/runtime/spdlog_logger.cpp
    src/runtime/values.cpp
)

if(UNIX)
    list(APPEND CVHUB_RUNTIME_SOURCES src/runtime/python_runtime.cpp)
else()
    list(APPEND CVHUB_RUNTIME_SOURCES src/runtime/python_runtime_stub.cpp)
endif()

add_library(cvhub_runtime ${CVHUB_RUNTIME_SOURCES})
target_include_directories(cvhub_runtime PUBLIC include)
target_link_libraries(cvhub_runtime PUBLIC cvhub_core spdlog::spdlog Threads::Threads)

add_library(cvhub_app
    src/app/application.cpp
    src/app/sample_graph.cpp
)
target_include_directories(cvhub_app PUBLIC include)
target_link_libraries(cvhub_app PUBLIC cvhub_runtime)

add_subdirectory(plugins/common)
target_link_libraries(cvhub_app PUBLIC cvhub_plugin_common)

if(CVHUB_BUILD_PLUGIN_OPENCV)
    add_subdirectory(plugins/opencv)
    target_link_libraries(cvhub_app PUBLIC cvhub_plugin_opencv)
    target_compile_definitions(cvhub_app PUBLIC CVHUB_HAS_OPENCV_PLUGIN=1)
endif()

if(CVHUB_BUILD_PLUGIN_PYTORCH)
    add_subdirectory(plugins/pytorch)
    target_link_libraries(cvhub_app PUBLIC cvhub_plugin_pytorch)
    target_compile_definitions(cvhub_app PUBLIC CVHUB_HAS_PYTORCH_PLUGIN=1)
endif()

add_executable(cvhub apps/cvhub/main.cpp)
target_link_libraries(cvhub PRIVATE cvhub_app)
set_target_properties(cvhub PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/cvhub")

if(CVHUB_BUILD_GUI)
    if(APPLE)
        enable_language(OBJCXX)
    elseif(WIN32)
        enable_language(RC)
    endif()
    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(imgui CONFIG QUIET)
    find_package(imnodes CONFIG QUIET)

    if(TARGET glfw)
        set(CVHUB_GLFW_TARGET glfw)
    elseif(TARGET glfw3::glfw)
        set(CVHUB_GLFW_TARGET glfw3::glfw)
    else()
        message(FATAL_ERROR "GLFW was found, but no usable CMake target was exported")
    endif()

    if(TARGET imgui::imgui)
        set(CVHUB_IMGUI_TARGET imgui::imgui)
    elseif(TARGET imgui)
        set(CVHUB_IMGUI_TARGET imgui)
    elseif(CVHUB_FETCH_GUI_DEPS)
        include(FetchContent)
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.92.8
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(imgui)
        add_library(cvhub_vendor_imgui
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        )
        target_include_directories(cvhub_vendor_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
        target_link_libraries(cvhub_vendor_imgui PUBLIC ${CVHUB_GLFW_TARGET} OpenGL::GL)
        set(CVHUB_IMGUI_TARGET cvhub_vendor_imgui)
    else()
        message(FATAL_ERROR "Dear ImGui was not found. Install an imgui CMake package or enable CVHUB_FETCH_GUI_DEPS.")
    endif()

    if(TARGET imnodes::imnodes)
        set(CVHUB_IMNODES_TARGET imnodes::imnodes)
    elseif(TARGET imnodes)
        set(CVHUB_IMNODES_TARGET imnodes)
    elseif(CVHUB_FETCH_GUI_DEPS)
        include(FetchContent)
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
        FetchContent_Declare(
            imnodes
            GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
            GIT_TAG eb36902
            GIT_SHALLOW FALSE
        )
        FetchContent_GetProperties(imnodes)
        if(NOT imnodes_POPULATED)
            cmake_policy(PUSH)
            if(POLICY CMP0169)
                cmake_policy(SET CMP0169 OLD)
            endif()
            FetchContent_Populate(imnodes)
            cmake_policy(POP)
        endif()
        add_library(cvhub_vendor_imnodes ${imnodes_SOURCE_DIR}/imnodes.cpp)
        target_include_directories(cvhub_vendor_imnodes PUBLIC ${imnodes_SOURCE_DIR})
        target_link_libraries(cvhub_vendor_imnodes PUBLIC ${CVHUB_IMGUI_TARGET})
        set(CVHUB_IMNODES_TARGET cvhub_vendor_imnodes)
        set(CVHUB_IMNODES_PROVIDES_IMGUI ON)
    else()
        add_library(cvhub_imnodes_compat third_party/imnodes_compat/imnodes.cpp)
        target_include_directories(cvhub_imnodes_compat PUBLIC third_party/imnodes_compat)
        target_link_libraries(cvhub_imnodes_compat PUBLIC ${CVHUB_IMGUI_TARGET})
        set(CVHUB_IMNODES_TARGET cvhub_imnodes_compat)
        set(CVHUB_IMNODES_PROVIDES_IMGUI ON)
    endif()

    find_package(nlohmann_json CONFIG QUIET)
    if(NOT TARGET nlohmann_json::nlohmann_json)
        include(FetchContent)
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
            GIT_SHALLOW TRUE
        )
        set(JSON_BuildTests OFF CACHE INTERNAL "")
        FetchContent_MakeAvailable(nlohmann_json)
    endif()

    set(CVHUB_GUI_SOURCES
        apps/cvhub_gui/main.cpp
    )
    if(APPLE)
        list(APPEND CVHUB_GUI_SOURCES apps/cvhub_gui/file_dialog_mac.mm)
    else()
        list(APPEND CVHUB_GUI_SOURCES apps/cvhub_gui/file_dialog_fallback.cpp)
    endif()
    if(WIN32)
        list(APPEND CVHUB_GUI_SOURCES apps/cvhub_gui/resources/cvhub_gui.rc)
    endif()

    add_executable(cvhub_gui MACOSX_BUNDLE ${CVHUB_GUI_SOURCES})
    target_include_directories(cvhub_gui PRIVATE apps/cvhub_gui)
    if(CVHUB_IMNODES_PROVIDES_IMGUI)
        target_link_libraries(cvhub_gui PRIVATE cvhub_app ${CVHUB_IMNODES_TARGET} ${CVHUB_GLFW_TARGET} OpenGL::GL nlohmann_json::nlohmann_json)
    else()
        target_link_libraries(cvhub_gui PRIVATE cvhub_app ${CVHUB_IMGUI_TARGET} ${CVHUB_IMNODES_TARGET} ${CVHUB_GLFW_TARGET} OpenGL::GL nlohmann_json::nlohmann_json)
    endif()
    if(APPLE)
        target_link_libraries(cvhub_gui PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers")
    elseif(WIN32)
        target_link_libraries(cvhub_gui PRIVATE ole32 uuid)
    endif()
    find_file(CVHUB_IMGUI_GLFW_BACKEND_SOURCE imgui_impl_glfw.cpp PATH_SUFFIXES backends imgui/backends NO_CACHE)
    find_file(CVHUB_IMGUI_OPENGL3_BACKEND_SOURCE imgui_impl_opengl3.cpp PATH_SUFFIXES backends imgui/backends NO_CACHE)
    if(CVHUB_IMGUI_GLFW_BACKEND_SOURCE AND CVHUB_IMGUI_OPENGL3_BACKEND_SOURCE AND NOT CVHUB_IMGUI_TARGET STREQUAL "cvhub_vendor_imgui")
        target_sources(cvhub_gui PRIVATE ${CVHUB_IMGUI_GLFW_BACKEND_SOURCE} ${CVHUB_IMGUI_OPENGL3_BACKEND_SOURCE})
    endif()
    if(APPLE)
        set(CVHUB_MAC_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app_icon.icns")
        if(EXISTS "${CVHUB_MAC_ICON}")
            target_sources(cvhub_gui PRIVATE ${CVHUB_MAC_ICON})
            set_source_files_properties(${CVHUB_MAC_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
            set_target_properties(cvhub_gui PROPERTIES
                OUTPUT_NAME CV_HUB
                MACOSX_BUNDLE_BUNDLE_NAME CV_HUB
                MACOSX_BUNDLE_GUI_IDENTIFIER com.cvhub.CV-HUB
                MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
                MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
                MACOSX_BUNDLE_ICON_FILE app_icon.icns
            )
        endif()
        add_custom_command(TARGET cvhub_gui PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f
                    "${CMAKE_BINARY_DIR}/apps/cvhub_gui/cvhub_gui"
            COMMAND ${CMAKE_COMMAND} -E rm -rf
                    "${CMAKE_BINARY_DIR}/apps/cvhub_gui/cvhub_gui.app"
            COMMENT "Removing stale non-bundled cvhub_gui launch artifacts"
        )
    else()
        set_target_properties(cvhub_gui PROPERTIES OUTPUT_NAME CV_HUB)
    endif()
    set_target_properties(cvhub_gui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/cvhub_gui")
endif()

if(CVHUB_BUILD_TESTS AND CVHUB_BUILD_PLUGIN_OPENCV)
    enable_testing()
    add_executable(cvhub_tests tests/runtime_tests.cpp)
    target_link_libraries(cvhub_tests PRIVATE cvhub_app)
    add_test(NAME cvhub_runtime_tests COMMAND cvhub_tests)

    add_executable(cvhub_python_node_tests tests/python_node_tests.cpp)
    target_link_libraries(cvhub_python_node_tests PRIVATE cvhub_runtime)
    target_include_directories(cvhub_python_node_tests PRIVATE plugins/pytorch/include plugins/pytorch/src)
    add_test(NAME cvhub_python_node_tests COMMAND cvhub_python_node_tests)
endif()
