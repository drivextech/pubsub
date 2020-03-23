cmake_minimum_required(VERSION 3.1.8)
project(pubsub VERSION 0.1.0)

option(SELF_TEST_ENABLE "If is standalone project? (not part of some project)" OFF)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(SELF_TEST_ENABLE)
    execute_process(
        COMMAND git clone https://github.com/drivextech/dxt_common
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_subdirectory(dxt_common)
endif()
unset(USE_MY_LIB CACHE)

add_subdirectory(Thirdparty/rabbitmq-c)
# add_subdirectory(Thirdparty/rabbitmq-cpp)

include_directories(${PROJECT_SOURCE_DIR}/Thirdparty/rabbitmq-c/librabbitmq)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(pubsub
    STATIC
    src/utils.cpp
    src/pubsub_backend.cpp
    src/pubsub_backend_rabbitmq.cpp)
add_dependencies(pubsub dxt_common)
target_link_libraries(pubsub dxt_common rabbitmq-static)

add_subdirectory(tests)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
