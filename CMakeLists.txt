cmake_minimum_required(VERSION 3.16)
project(OptimalRepair)

set(GLPK_INCLUDE_DIR /usr/local/include/)
set(GLPK_INCLUDE_DIR /usr/arm-linux-gnueabi/include)
set(GLPK_LIBRARIES /usr/local/lib/libglpk.so)
set(ISA_ARM_LIBRARIES /usr/local/lib/armlibisal.so) 

include_directories(${GLPK_INCLUDE_DIR})

# Add header file directory to the search path
include_directories(include)

# Add source files to source directory
aux_source_directory(src/client CLIENT_SOURCES)
aux_source_directory(src/storagenodes storagenodes_SOURCES)
aux_source_directory(src/pnetdevice pnetdevice_SOURCES)
aux_source_directory(src/share_func share_func_SOURCES)

# Set compilation options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -pthread -lm -g")

if(NOT CMAKE_TOOLCHAIN_FILE)
    # Add source directory to executable files 
    add_executable(client ${share_func_SOURCES} ${CLIENT_SOURCES})
    add_executable(storagenodes ${share_func_SOURCES} ${storagenodes_SOURCES})
    add_executable(pnetdevice ${share_func_SOURCES} ${pnetdevice_SOURCES})

    # Set the output directory for executables
    set_target_properties(client PROPERTIES RUNTIME_OUTPUT_DIRECTORY build)
    set_target_properties(storagenodes PROPERTIES RUNTIME_OUTPUT_DIRECTORY build)
    set_target_properties(pnetdevice PROPERTIES RUNTIME_OUTPUT_DIRECTORY build)

    # Link libraries
    target_link_libraries(client isal ${GLPK_LIBRARIES})
    target_link_libraries(storagenodes isal)
    target_link_libraries(pnetdevice isal)
endif()

# Use arm toolchain for pnetdevice_arm
if(CMAKE_TOOLCHAIN_FILE)
    include("${CMAKE_TOOLCHAIN_FILE}")
    add_executable(pnetdevice_arm ${share_func_SOURCES} ${pnetdevice_SOURCES})
    set_target_properties(pnetdevice_arm PROPERTIES RUNTIME_OUTPUT_DIRECTORY build)
    target_link_libraries(pnetdevice_arm ${ISA_ARM_LIBRARIES})
endif()
