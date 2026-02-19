cmake_minimum_required(VERSION 3.16)
project(BMO_Backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(bmo_backend ${SOURCES})

find_package(llama CONFIG REQUIRED)

target_link_libraries(bmo_backend PRIVATE llama)

target_include_directories(bmo_backend PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

set_target_properties(bmo_backend PROPERTIES
  BUILD_RPATH "\$ORIGIN/lib"
  INSTALL_RPATH "\$ORIGIN/lib"
)