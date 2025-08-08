# add_executable(simple_example simple_example.cpp)
# target_link_libraries(simple_example kvdb)

# add_executable(structure_test structure_test.cpp)
# target_link_libraries(structure_test kvdb)


# add_executable(tp_test tp_test.cpp)
# target_link_libraries(tp_test kvdb)

add_executable(async_db_example async_db_example.cpp)
target_link_libraries(async_db_example kvdb)

add_executable(performance_example performance_example.cpp)
target_link_libraries(performance_example kvdb)
