# convert_proto_struct

[中文](./README.md) | English

#### Introduction
Convert 'Protobuf message' to 'c++ struct', or convert 'c++ struct' to 'Protobuf message'.

Support Protobuf Type:
1. Fundamental types, such as int32, int64, bool, Enum, string and so on.
2. Containers, repeated or map

Unsupport: oneof

Support struct:
1. Do NOT set alignment for struct.
2. Fundamental types, such as int, int64_t, bool, enum and so on.
3. String should be std::string.
4. The repeated should be std::vector.
5. The map should be std::map

Unsupport: bit field.

#### Usage
Refer the example in test.

