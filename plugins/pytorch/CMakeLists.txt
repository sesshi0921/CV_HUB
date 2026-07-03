find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ---------------------------------------------------------------------------
# Resolve Python venv via pyproject.toml
# Priority: CVHUB_PYTORCH_PYTHON cache var > uv sync (preferred) > pip > fallback
# ---------------------------------------------------------------------------
set(CVHUB_PYTORCH_PYTHON "" CACHE FILEPATH "Python interpreter with torchvision installed (auto-detected)")
set(_VENV_DIR "${CMAKE_SOURCE_DIR}/.venv")
set(_VENV_PYTHON "${_VENV_DIR}/bin/python3")

if(NOT CVHUB_PYTORCH_PYTHON)
    find_program(UV_EXECUTABLE uv)

    if(UV_EXECUTABLE)
        message(STATUS "PyTorch plugin: syncing venv via uv (pyproject.toml)")
        execute_process(
            COMMAND ${UV_EXECUTABLE} sync --project "${CMAKE_SOURCE_DIR}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE _uv_result
            OUTPUT_QUIET ERROR_QUIET
        )
        if(_uv_result EQUAL 0 AND EXISTS "${_VENV_PYTHON}")
            set(CVHUB_PYTORCH_PYTHON "${_VENV_PYTHON}")
        else()
            message(WARNING "uv sync failed (exit ${_uv_result}), falling back")
        endif()
    endif()

    # Fallback: try known venv candidates
    if(NOT CVHUB_PYTORCH_PYTHON)
        foreach(_cand IN ITEMS
            "$ENV{VIRTUAL_ENV}/bin/python3"
            "$ENV{HOME}/.venv/melchior/bin/python3"
        )
            if(EXISTS "${_cand}")
                execute_process(
                    COMMAND "${_cand}" -c "import torchvision"
                    RESULT_VARIABLE _tv_result OUTPUT_QUIET ERROR_QUIET
                )
                if(_tv_result EQUAL 0)
                    set(CVHUB_PYTORCH_PYTHON "${_cand}")
                    break()
                endif()
            endif()
        endforeach()
    endif()

    if(NOT CVHUB_PYTORCH_PYTHON)
        message(FATAL_ERROR
            "PyTorch plugin: could not find a Python with torchvision.\n"
            "Install uv and run cmake again, or set CVHUB_PYTORCH_PYTHON manually.")
    endif()
endif()

message(STATUS "PyTorch plugin: using Python ${CVHUB_PYTORCH_PYTHON}")

# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------
set(GEN_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pytorch_auto_nodes.cpp)

add_custom_command(
    OUTPUT  ${GEN_OUTPUT}
    COMMAND "${CVHUB_PYTORCH_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_nodes.py"
            --output "${GEN_OUTPUT}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_nodes.py"
    COMMENT "Generating PyTorch auto nodes from torchvision.transforms.functional"
)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(cvhub_plugin_pytorch
    src/pytorch_plugin.cpp
    ${GEN_OUTPUT}
)

target_include_directories(cvhub_plugin_pytorch
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(cvhub_plugin_pytorch PUBLIC cvhub_runtime)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/pytorch_worker.py"
    "${CMAKE_BINARY_DIR}/pytorch_worker.py"
    COPYONLY
)

target_compile_definitions(cvhub_plugin_pytorch PUBLIC
    CVHUB_PYTORCH_ENABLED=1
    CVHUB_PYTORCH_PYTHON="${CVHUB_PYTORCH_PYTHON}"
    CVHUB_PYTORCH_WORKER_PATH="${CMAKE_BINARY_DIR}/pytorch_worker.py"
)
