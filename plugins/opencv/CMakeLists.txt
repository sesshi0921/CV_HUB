find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

add_library(cvhub_plugin_opencv
    src/opencv_plugin.cpp
)
target_include_directories(cvhub_plugin_opencv PUBLIC include)
target_include_directories(cvhub_plugin_opencv PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(cvhub_plugin_opencv PUBLIC cvhub_runtime ${OpenCV_LIBS})
target_compile_definitions(cvhub_plugin_opencv PUBLIC CVHUB_OPENCV_ENABLED=1)
