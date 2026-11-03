cmake_minimum_required(VERSION 3.16.3)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED True)

project (NetworkLib)

# 线程支持。
set(LIBS
    pthread
)

find_package(Threads REQUIRED)

# 开启 ASan 内存检测（仅调试使用）
add_compile_options(-fsanitize=address -g)
add_link_options(-fsanitize=address)

add_subdirectory(src)

add_executable(testserver example/testserver.cc)
target_link_libraries(testserver PRIVATE ${CMAKE_PROJECT_NAME})
