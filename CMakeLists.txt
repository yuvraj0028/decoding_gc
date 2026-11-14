cmake_minimum_required(VERSION 3.16)
project(decoding_gc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(gc INTERFACE)
target_include_directories(gc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(gc INTERFACE cxx_std_17)

function(gc_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

add_executable(gc_demo src/demo.cpp)
target_link_libraries(gc_demo PRIVATE gc)
gc_set_warnings(gc_demo)

add_executable(gc_tests tests/gc_tests.cpp)
target_link_libraries(gc_tests PRIVATE gc)
gc_set_warnings(gc_tests)

enable_testing()
add_test(NAME gc_tests COMMAND gc_tests)
