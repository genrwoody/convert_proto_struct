# convert_proto_struct

[中文](./README.md) | [English](./README.en-US.md)

#### Introduction
Convert 'Protobuf message' to 'c++ struct', or convert 'c++ struct' to 'Protobuf message'.

Support Protobuf Type:
1. Fundamental types, such as int32, int64, bool, Enum, string and so on.
2. Containers, repeated or map, the map key type should be string.

Unsupport: oneof, the map key type is not string

Support struct:
1. Must align as 1 byte.
2. Fundamental types, such as int, int64_t, bool, enum and so on.
3. String should be std::string.
4. The repeated should be std::vector.
5. The map should be std::map, the map key type should be std::string.

Unsupport: bit field.

#### Usage
Refer the example in test.

