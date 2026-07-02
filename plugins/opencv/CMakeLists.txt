find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio photo)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ── auto-generate node registrations from OpenCV headers ─────────────────────
set(CVHUB_OPENCV_GEN_HEADERS
    "${OpenCV_INCLUDE_DIRS}/opencv2/imgproc.hpp"
    "${OpenCV_INCLUDE_DIRS}/opencv2/photo.hpp"
)
set(CVHUB_OPENCV_GEN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/opencv_auto_nodes.cpp")

add_custom_command(
    OUTPUT  ${CVHUB_OPENCV_GEN_OUTPUT}
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_nodes.py"
            --headers ${CVHUB_OPENCV_GEN_HEADERS}
            --includes opencv2/imgproc.hpp opencv2/photo.hpp
            --output   ${CVHUB_OPENCV_GEN_OUTPUT}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_nodes.py"
            ${CVHUB_OPENCV_GEN_HEADERS}
    COMMENT "Generating OpenCV auto nodes from headers"
)

add_library(cvhub_plugin_opencv
    src/opencv_plugin.cpp
    ${CVHUB_OPENCV_GEN_OUTPUT}
)
target_include_directories(cvhub_plugin_opencv PUBLIC  include)
target_include_directories(cvhub_plugin_opencv PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    src   # for opencv_helpers.hpp
    ${CMAKE_CURRENT_BINARY_DIR}  # for generated file to find opencv_helpers.hpp
)
target_link_libraries(cvhub_plugin_opencv PUBLIC cvhub_runtime ${OpenCV_LIBS})
target_compile_definitions(cvhub_plugin_opencv PUBLIC CVHUB_OPENCV_ENABLED=1)
