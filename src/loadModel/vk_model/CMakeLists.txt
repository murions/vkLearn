cmake_minimum_required(VERSION 3.16)
project(vk_model)

# add executable
add_executable(${PROJECT_NAME})

add_compile_definitions(CURRENT_FILE_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_compile_definitions(ASSET_SOURCE_DIR="${VKLEARN_TOP_LEVEL_DIR}/img/")

target_sources(${PROJECT_NAME}
	PRIVATE
		main.cpp
)