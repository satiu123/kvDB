add_executable(simple_example simple_example.cpp)
target_link_libraries(simple_example kvdb)

add_executable(structure_test structure_test.cpp)
target_link_libraries(structure_test kvdb)

add_executable(coro_test coro_test.cpp)
target_link_libraries(coro_test kvdb)

add_executable(tp_test tp_test.cpp)
target_link_libraries(tp_test kvdb)