cmake_minimum_required(VERSION 3.16)
project(vk_inflight)

# add executable
add_executable(${PROJECT_NAME})

add_compile_definitions(CURRENT_FILE_DIR="${CMAKE_CURRENT_LIST_DIR}")

target_sources(${PROJECT_NAME}
	PRIVATE
		main.cpp
)