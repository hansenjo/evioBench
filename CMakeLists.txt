cmake_minimum_required(VERSION 3.5)

project(evioBench LANGUAGES CXX)

# EVIO prerequisite. If not installed, download and build it.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
find_package(EVIO CONFIG QUIET)
if(NOT EVIO_FOUND)
  find_package(EVIO MODULE)
  if(NOT EVIO_FOUND)
    include(FetchBuildEVIO)
  endif()
endif()

set(CMAKE_CXX_STANDARD 14)

add_executable(evioBench evioBench.cc)
target_link_libraries(evioBench PRIVATE EVIO::EVIO)
