// Copyright 2021 genrwoody@163.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _CONVERT_PROTO_STRUCT_INC_
#define _CONVERT_PROTO_STRUCT_INC_

#include <cstddef>
#include <cstdint>

namespace google { namespace protobuf { class Message; } }

namespace cps
{

using google::protobuf::Message;

// @brief Convert struct to protobuf message.
// @param[in] bytes: pointer to struct
// @param[in] size: sizeof struct
// @param[out] msg: protobuf message
// @return true for success, or false for failed.
bool StructToProto(const void *bytes, size_t size, Message &msg);

// @brief Convert protobuf message to struct.
// @param[in] msg: protobuf message
// @param[out] bytes: pointer to struct
// @param[in] size: sizeof struct
// @return true for success, or false for failed.
bool ProtoToStruct(const Message &msg, void *bytes, size_t size);

// @brief Convert struct to protobuf message.
template<typename STRUCT, typename PROTO>
bool StructToProto(const STRUCT &in, PROTO &out)
{
	return StructToProto(&in, sizeof(STRUCT), out);
}

// @brief Convert protobuf message to struct.
template<typename PROTO, typename STRUCT>
bool ProtoToStruct(const PROTO &in, STRUCT &out)
{
	return ProtoToStruct(in, &out, sizeof(STRUCT));
}

} // namespace cps

#endif // _CONVERT_PROTO_STRUCT_INC_
