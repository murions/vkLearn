cmake_minimum_required(VERSION 3.14)
project(vkLearn)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# unzip
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/ext)
execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/ext/glfw-3.3.9.zip
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext
)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/ext)
execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/ext/glm.zip
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext
)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/ext)
execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/ext/vulkan.zip
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext
)

# set path
set(GLFW_BUILD_DIRECTORY ${CMAKE_BINARY_DIR}/ext/glfw-3.3.9/build)
set(GLM_INCLUDE_DIRECTORY ${CMAKE_BINARY_DIR}/ext/glm)
set(VULKAN_INCLUDE_DIRECTORY ${CMAKE_BINARY_DIR}/ext/vulkan)

# set path micro
add_compile_definitions(HOME_DIR="${CMAKE_SOURCE_DIR}")

# glfw build
if(WIN32)
    if(MSVC)
        execute_process(COMMAND ${CMAKE_COMMAND} -B ${GLFW_BUILD_DIRECTORY}
            -S ${CMAKE_BINARY_DIR}/ext/glfw-3.3.9/
            -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/glfw-3.3.9
        )
    elseif(MINGW)
        execute_process(COMMAND ${CMAKE_COMMAND} -B ${GLFW_BUILD_DIRECTORY}
            -S ${CMAKE_BINARY_DIR}/ext/glfw-3.3.9/
            -G "MinGW Makefiles"
            -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/glfw-3.3.9
        )
    endif()
elseif(LINUX)
    execute_process(COMMAND ${CMAKE_COMMAND} -B ${GLFW_BUILD_DIRECTORY}
        -S ${CMAKE_BINARY_DIR}/ext/glfw-3.3.9/
        -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/glfw-3.3.9
    )
endif()

# glfw make
execute_process(COMMAND ${CMAKE_COMMAND} --build ${GLFW_BUILD_DIRECTORY}
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext/glfw-3.3.9
)

add_subdirectory(src)