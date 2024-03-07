cmake_minimum_required(VERSION 3.16)

# include FetchContent

include(FetchContent)

# declare third party library

Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.10
    URL_HASH 5e4ae02dc7c9b084232824c2511679a7e0b0b09f2bae70191ad9703691368b58
    GIT_PROGRESS TRUE
)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
    URL_HASH 4605259c22feadf35388c027f07b345ad3aa3b12631a5a316347f7566c6f1839
    GIT_PROGRESS TRUE
)

FetchContent_Declare(glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG 12.3.1
    URL_HASH c1901f2c236f454b2f77a5e73af51dd934b50d08929aac536e1af9d39a2d7307
    GIT_PROGRESS TRUE
)

# set glfw build option

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# set glslang build option

set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ALLOW_EXTERNAL_SPIRV_TOOLS OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)


# download and build glfw, glm, glslang

FetchContent_MakeAvailable(glfw glm glslang)
