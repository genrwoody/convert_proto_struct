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

#include <google/protobuf/message.h>

namespace cps
{

using google::protobuf::Message;
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Reflection;

// internal function
int GetMessageStructSize(const Descriptor *desc);
bool StructToProtoInternal(const uint8_t *&bytes, Message &msg);
bool ProtoToStructInternal(uint8_t *&bytes, const Message &msg, int start);

// @brief Convert struct to protobuf message.
template<typename STRUCT, typename PROTO>
bool StructToProto(const STRUCT &in, PROTO &out)
{
	int size = GetMessageStructSize(out.GetDescriptor());
	// The struct should match protobuf, and should pack 1.
	if (size != sizeof(STRUCT)) return false;
	auto bytes = (const uint8_t*)&in;
	return StructToProtoInternal(bytes, out);
}

// @brief Convert protobuf message to struct.
template<typename PROTO, typename STRUCT>
bool ProtoToStruct(const PROTO &in, STRUCT &out)
{
	int size = GetMessageStructSize(in.GetDescriptor());
	// The struct should match protobuf, and should pack 1.
	if (size != sizeof(STRUCT)) return false;
	auto bytes = (uint8_t*)&out;
	return ProtoToStructInternal(bytes, in, 0);
}

} // namespace cps

#endif // _CONVERT_PROTO_STRUCT_INC_

