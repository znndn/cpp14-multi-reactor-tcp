file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR} "*.cc")

add_library(${CMAKE_PROJECT_NAME} SHARED ${SRC_FILES})

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC Threads::Threads)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)
