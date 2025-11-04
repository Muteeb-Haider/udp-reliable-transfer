# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Windows-specific settings
if(WIN32)
    set(WINSOCK_LIB ws2_32)
endif()

add_library(ruft_common
    common/protocol.c
    common/crc32.c
    common/util.c
)
target_include_directories(ruft_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(client client/main.c)
target_link_libraries(client PRIVATE ruft_common)
if(WIN32)
    target_link_libraries(client PRIVATE ${WINSOCK_LIB})
endif()

add_executable(server server/main.c)
target_link_libraries(server PRIVATE ruft_common)
if(WIN32)
    target_link_libraries(server PRIVATE ${WINSOCK_LIB})
endif()

# Output to build/bin
set_target_properties(client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set_target_properties(server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
