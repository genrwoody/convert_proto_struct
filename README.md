# convert_proto_struct

#### 介绍
转换Protobuf消息类为c++结构体, 或者转换c++结构体为Protobuf消息类

支持的Protobuf类型:
1. 基本类型, 如int32, int64, bool, Enum, string等
2. 容器, repeated, 以string做为key的map

不支持: oneof, 以非string做为key的map

支持的结构体类型:
1. 结构体必须按1字节对齐
2. 基本类型, 如int, int64_t, bool, enum等
3. 字符串必须是std::string
4. repeated对应的类型为std::vector
5. map对应的类型必须是std::map, key也必须是string

不支持: 位域

使用方法可参考test中的示例

