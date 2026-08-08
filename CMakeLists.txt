cmake_minimum_required(VERSION 4.0)
project(vkpbr)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.4 REQUIRED)
find_package(glm REQUIRED)

function (add_shader_target TARGET)
    set(ARGS VERTEX FRAGMENT)
    cmake_parse_arguments("arg" "" "" "${ARGS}" ${ARGN})
    set(SLANG_ARGS --target-env=vulkan1.0)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/vert.spv
        COMMAND glslc
                ${arg_VERTEX}
                ${SLANG_ARGS}
                -o ${CMAKE_BINARY_DIR}/vert.spv
        DEPENDS ${arg_VERTEX}
        COMMENT "Compiling vertex shader"
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/frag.spv
        COMMAND glslc
                ${arg_FRAGMENT}
                ${SLANG_ARGS}
                -o ${CMAKE_BINARY_DIR}/frag.spv
        DEPENDS ${arg_FRAGMENT}
        COMMENT "Compiling fragment shader"
        VERBATIM
    )
    add_custom_target(${TARGET}
        DEPENDS ${CMAKE_BINARY_DIR}/vert.spv
                ${CMAKE_BINARY_DIR}/frag.spv
    )
endfunction()

add_shader_target(shaders
    VERTEX ${CMAKE_SOURCE_DIR}/src/shaders/shader.vert
    FRAGMENT ${CMAKE_SOURCE_DIR}/src/shaders/shader.frag
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/engine.cpp
)
add_dependencies(${PROJECT_NAME} shaders)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

if (DEFINED ENV{VK_INCLUDE_DIR})
    target_include_directories(${PROJECT_NAME} PRIVATE
        $ENV{VK_INCLUDE_DIR}
    )
endif()

if (DEBUG)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -DDEBUG
        -g
        -fno-omit-frame-pointer
        -O0
        -fsanitize=address
        -Wall
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address
    )
endif()

