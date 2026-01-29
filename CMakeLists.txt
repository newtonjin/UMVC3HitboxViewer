cmake_minimum_required(VERSION 3.15)
project(UMVC3Overlay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform specific settings
if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# Include directories
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	
)

# Source files for the main DLL
set(MAIN_SOURCES
	src/dllmain.cpp
	src/dx9hook.cpp
	src/renderer.cpp
	src/imgui_d3d9.cpp
	src/character.cpp
)

# Find DirectX SDK (optional, can be provided by Visual Studio)
find_package(DirectX)

# Create the DLL
add_library(UMVC3Overlay SHARED ${MAIN_SOURCES})

# Link libraries
target_link_libraries(UMVC3Overlay 
	d3d9
	d3dx9
)

# Set output directory
set_target_properties(UMVC3Overlay PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# Additional configuration for Visual Studio
if(MSVC)
	# Set subsystem to Windows
	set_target_properties(UMVC3Overlay PROPERTIES
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
	)
endif()

# Print configuration
message(STATUS "UMVC3 Overlay DLL Build Configuration")
message(STATUS "Platform: ${CMAKE_SYSTEM}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
