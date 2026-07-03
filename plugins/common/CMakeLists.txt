find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc videoio)

add_library(cvhub_plugin_common
    src/common_plugin.cpp
)
target_include_directories(cvhub_plugin_common PUBLIC include)
target_include_directories(cvhub_plugin_common PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    src
)
target_link_libraries(cvhub_plugin_common PUBLIC cvhub_runtime ${OpenCV_LIBS})
target_compile_definitions(cvhub_plugin_common PUBLIC CVHUB_COMMON_ENABLED=1)
