cmake_minimum_required(VERSION 3.28)

# Get all cpp files in current directory
file(GLOB CPP_FILES "*.cpp")

# Create executable target for each cpp file
foreach(CPP_FILE ${CPP_FILES})
    # Get filename without extension
    get_filename_component(TARGET_NAME ${CPP_FILE} NAME_WE)
    
    # Create executable target
    add_executable(${TARGET_NAME} ${CPP_FILE})
    
    # Optional: link libraries if needed
    target_link_libraries(${TARGET_NAME} kvdb)
endforeach()

