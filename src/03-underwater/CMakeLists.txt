cmake_minimum_required(VERSION 3.10)
cmake_policy(VERSION 3.10)
project("HW2" C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(PROJECT_ORIGIN "../../")

file(GLOB SOURCE
    "src/*.h"
    "src/animation/*.h"
    "src/*.cpp"
)

set(SOURCE ${SOURCE} ../glad.c)
set(NAME "main")

link_directories(${PROJECT_ORIGIN}/lib)

if(WIN32)
  set(LIBS glfw3 opengl32 freetype assimp FreeImage)
  file(COPY ${PROJECT_ORIGIN}/dlls/assimp-vc140-mt.dll
     DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  file(COPY ${PROJECT_ORIGIN}/dlls/FreeImage.dll
     DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif(WIN32)

if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_search_module(GLFW REQUIRED glfw3)
  find_package(glm REQUIRED)
  find_package(assimp REQUIRED)
  find_library(FREETYPE_LIBRARY NAMES freetype)
  find_library(ASSIMP_LIBRARY NAMES assimp)
  find_path(FREEIMAGE_INCLUDE_DIR FreeImage.h)
  find_library(FREEIMAGE_LIBRARY NAMES freeimage FreeImage)
  if(NOT FREETYPE_LIBRARY OR NOT ASSIMP_LIBRARY OR NOT FREEIMAGE_INCLUDE_DIR OR NOT FREEIMAGE_LIBRARY)
    message(FATAL_ERROR "freetype, assimp, and FreeImage are required.")
  endif()
  set(LIBS "-L${GLFW_LIBRARY_DIRS}" ${GLFW_STATIC_LIBRARIES} ${FREETYPE_LIBRARY} ${ASSIMP_LIBRARY} ${FREEIMAGE_LIBRARY})
  include_directories(${GLFW_INCLUDE_DIRS})
  include_directories(${GLM_INCLUDE_DIRS})
  include_directories(${ASSIMP_INCLUDE_DIRS})
  include_directories(${FREEIMAGE_INCLUDE_DIR})
endif(UNIX)

list(APPEND CMAKE_CXX_FLAGS "-std=c++11")
add_executable(${NAME} ${SOURCE})
if(UNIX)
  target_compile_options(${NAME} PRIVATE "-I${FREEIMAGE_INCLUDE_DIR}")
endif(UNIX)
target_link_libraries(${NAME} ${LIBS})
include_directories(${PROJECT_ORIGIN}/includes)
