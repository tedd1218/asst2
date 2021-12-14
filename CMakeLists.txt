
cmake_minimum_required(VERSION 2.4)

project(CameraPipeline)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Lib output dir")
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Binary output dir")

# defaulting to release build
IF (NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE         "Release" CACHE STRING "Type of build (Debug Release)" FORCE)
ENDIF ()

#add_definitions(-D_DEBUG)
if(CMAKE_COMPILER_IS_GNUCXX)
   set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS} -ftest-coverage -msse4.1")
endif()

include_directories(${CameraPipeline_SOURCE_DIR})
add_subdirectory(CoreLib)
add_subdirectory(CameraPipeline)
