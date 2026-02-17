cmake_minimum_required(VERSION 3.16)
project(BMO-Backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(portaudio CONFIG REQUIRED)
find_package(whisper-cpp CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(bmo-backend ${SOURCES})

target_include_directories(bmo-backend PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(bmo-backend PRIVATE
  whisper-cpp::whisper-cpp
)

if(TARGET portaudio)
  target_link_libraries(bmo-backend PRIVATE portaudio)
elseif(TARGET PortAudio::PortAudio)
  target_link_libraries(bmo-backend PRIVATE PortAudio::PortAudio)
else()
  message(FATAL_ERROR "No known PortAudio target found (portaudio or PortAudio::PortAudio).")
endif()
