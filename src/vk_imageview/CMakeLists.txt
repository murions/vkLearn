cmake_minimum_required(VERSION 3.14)
project(vk_imageView)

# add executable
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
	PRIVATE
		main.cpp
)
