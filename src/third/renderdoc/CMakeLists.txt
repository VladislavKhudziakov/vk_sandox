
add_library(renderdoc INTERFACE)

function (make_renderdoc_lib_win)
    set(ONE_VAL_ARGS ROOT_DIR)
    cmake_parse_arguments(RDOC_LIB "${OPTIONS}" "${ONE_VAL_ARGS}" "${MULTI_VAL_ARGS}" ${ARGN})

    file(TO_CMAKE_PATH ${RDOC_LIB_ROOT_DIR} RENDERDOC_CMAKE_PATH)
    string(REPLACE "/" "\\\\" RENDERDOC_DEFINE_PATH ${RENDERDOC_CMAKE_PATH})

    target_include_directories(renderdoc INTERFACE ${RENDERDOC_CMAKE_PATH})
    # TODO
#    target_compile_definitions(renderdoc INTERFACE SANDBOX_RENDERDOC_LIB_PATH="${RENDERDOC_DEFINE_PATH}\\renderdoc.dll")
    target_compile_definitions(renderdoc INTERFACE SANDBOX_RENDERDOC_LIB_PATH="renderdoc.dll")
endfunction()

if(NOT $ENV{BNB_RENDERDOC_DIR} STREQUAL "")
    make_renderdoc_lib_win(ROOT_DIR $ENV{BNB_RENDERDOC_DIR})
elseif(NOT $ENV{RENDERDOC_ROOT} STREQUAL "")
    make_renderdoc_lib_win(ROOT_DIR $ENV{RENDERDOC_ROOT})
endif()