cmake_minimum_required (VERSION 2.8.8)
project (wuppertools)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# TODO: Remove dependency on libpacketformat
include_directories("." "../driver/include")

file(GLOB WUPPERTOOLS_SOURCES *.c)
add_library(wupper ${WUPPERTOOLS_SOURCES})

add_executable(wupper-config wupper-config.c)
target_link_libraries(wupper-config 	wupper)

add_executable(wupper-dma-test wupper-dma-test.cpp)
target_link_libraries(wupper-dma-test 	wupper)

add_executable(wupper-info wupper-info.c)
target_link_libraries(wupper-info 	wupper)

add_executable(wupper-irq-test wupper-irq-test.c)
target_link_libraries(wupper-irq-test 	wupper)

add_executable(wupper-throughput wupper-throughput.c)
target_link_libraries(wupper-throughput 	wupper rt)

add_executable(wupper-reset wupper-reset.c)
target_link_libraries(wupper-reset 	wupper)

