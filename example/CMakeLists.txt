set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts/example")

add_executable(thunderscope_test thunderscope_test.cpp)
target_link_libraries(thunderscope_test tslitex_static)

add_executable(thunderscope_cal thunderscope_cal.cpp)
target_link_libraries(thunderscope_cal tslitex_static)

if(WIN32)
    target_link_libraries(thunderscope_test setupapi)
    target_link_libraries(thunderscope_cal setupapi)
endif()
