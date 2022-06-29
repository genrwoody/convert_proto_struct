cmake_minimum_required(VERSION 3.2)

project(convert_proto_struct)

find_package(protobuf CONFIG REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    test/message.proto
)

include_directories(src)

add_executable(test_cps
    test/main.cpp
    test/message.h
    src/convert_proto_struct.cpp
    src/convert_proto_struct.h
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(test_cps PRIVATE protobuf::libprotobuf)

