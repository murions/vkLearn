cmake_minimum_required(VERSION 3.14)
project(vkLearn)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# mkdir
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/ext)

#set global path
set(VKLEARN_BINARY_DIRECTORY ${CMAKE_BINARY_DIR})

# download third-party library
add_subdirectory(ext)

execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/ext/vulkan.zip
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext
)

# set third-party path
set(VULKAN_INCLUDE_DIRECTORY ${CMAKE_BINARY_DIR}/ext/vulkan)

# set path macro
add_compile_definitions(HOME_DIR="${CMAKE_SOURCE_DIR}")

add_subdirectory(src)
