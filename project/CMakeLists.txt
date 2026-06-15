cmake_minimum_required(VERSION 3.10)
cmake_policy(VERSION 3.10)
project("HW2" C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(PROJECT_ORIGIN "../")

file(GLOB SOURCE
    "src/*.h"
    "src/*.cpp"
)

set(SOURCE ${SOURCE} glad.c)
list(APPEND SOURCE
    src/shared/stb_image_impl.cpp
    src/shared/scene_transition_effect.cpp
    src/0-base/scene.cpp
    src/1-volcano/scene.cpp
    src/1-volcano/volcano_trajectory.cpp
    src/2-desert/scene.cpp
    src/3-underwater/scene.cpp
)
set(NAME "main")

link_directories(${PROJECT_ORIGIN}/lib)

if(WIN32)
  set(LIBS glfw3 opengl32 freetype assimp)
  file(COPY ${PROJECT_ORIGIN}/dlls/assimp-vc140-mt.dll
     DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif(WIN32)

if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_search_module(GLFW REQUIRED glfw3)
  find_package(glm REQUIRED)
  find_package(assimp REQUIRED)
  find_library(FREETYPE_LIBRARY NAMES freetype)
  find_library(ASSIMP_LIBRARY NAMES assimp)
  if(NOT FREETYPE_LIBRARY OR NOT ASSIMP_LIBRARY)
    message(FATAL_ERROR "freetype and assimp are required.")
  endif()
  get_filename_component(ASSIMP_LIBRARY_DIR ${ASSIMP_LIBRARY} DIRECTORY)
  get_filename_component(ASSIMP_PREFIX ${ASSIMP_LIBRARY_DIR} DIRECTORY)
  get_filename_component(ASSIMP_LIBRARY_REALPATH ${ASSIMP_LIBRARY} REALPATH)
  get_filename_component(ASSIMP_LIBRARY_REAL_DIR ${ASSIMP_LIBRARY_REALPATH} DIRECTORY)
  get_filename_component(ASSIMP_REAL_PREFIX ${ASSIMP_LIBRARY_REAL_DIR} DIRECTORY)
  set(LIBS "-L${GLFW_LIBRARY_DIRS}" ${GLFW_STATIC_LIBRARIES} ${FREETYPE_LIBRARY} ${ASSIMP_LIBRARY})
  include_directories(${GLFW_INCLUDE_DIRS})
  include_directories(${GLM_INCLUDE_DIRS})
  include_directories(BEFORE ${ASSIMP_REAL_PREFIX}/include)
  include_directories(BEFORE ${ASSIMP_PREFIX}/include)
  include_directories(BEFORE ${ASSIMP_INCLUDE_DIRS})
endif(UNIX)

add_executable(${NAME} ${SOURCE})
target_compile_definitions(${NAME} PRIVATE COMBINED_SCENE_APP GRAPHICS_PROJECT_NO_STB_IMAGE_IMPLEMENTATION)
target_link_libraries(${NAME} ${LIBS})
include_directories(src)
include_directories(${PROJECT_ORIGIN}/includes)
