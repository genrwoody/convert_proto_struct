#include "convert_proto_struct.h"

#include <string>
#include <vector>
#include <map>
#include <google/protobuf/message.h>

using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Reflection;

namespace cps
{

// for protobuf enum, different from int
struct Enum
{
	int value;
	Enum() : value(0) {}
	Enum(int v) : value(v) {}
	operator int() const { return value; }
};

template<typename Ty>
inline int GetFieldSize(const FieldDescriptor *field)
{
	return field->is_repeated() ? sizeof(std::vector<Ty>) : sizeof(Ty);
}

// @brief Get the size of struct which is match the protobuf message.
static int GetMessageStructSize(const Descriptor *desc)
{
	static_assert(sizeof(std::string) % 8 == 0,
		"error: sizeof std::string is not align as 64 bit.");
	static_assert(sizeof(std::vector<char>) % 8 == 0 &&
		sizeof(std::vector<char>) == sizeof(std::vector<char[97]>),
		"error: sizeof std::vector is variable, can not deal with it.");
	static_assert(sizeof(std::map<char, char>) % 8 == 0 &&
		sizeof(std::map<char, char>) == sizeof(std::map<char[97], char[97]>),
		"error: sizeof std::map is variable, can not deal with it.");

	int size = 0;
	int count = desc->field_count();
	for (int i = 0; i < count; ++i) {
		auto field = desc->field(i);
		switch (field->cpp_type()) {
		case FieldDescriptor::CPPTYPE_INT32:
		case FieldDescriptor::CPPTYPE_UINT32:
		case FieldDescriptor::CPPTYPE_FLOAT:
			size += GetFieldSize<int32_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_INT64:
		case FieldDescriptor::CPPTYPE_UINT64:
		case FieldDescriptor::CPPTYPE_DOUBLE:
			size += GetFieldSize<int64_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_BOOL:
			size += GetFieldSize<bool>(field);
			break;
		case FieldDescriptor::CPPTYPE_ENUM:
			size += GetFieldSize<int>(field);
			break;
		case FieldDescriptor::CPPTYPE_STRING:
			size += GetFieldSize<std::string>(field);
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:
		{
			if (field->is_map()) {
				size += sizeof(std::map<char, char>);
			} else if (field->is_repeated()) {
				size += sizeof(std::vector<char>);
			} else {
				int subsize = GetMessageStructSize(field->message_type());
				if (subsize == 0) return 0;
				size += subsize;
			}
			break;
		}
		default:
			return 0; // never reached!
		}
	}
	return size;
}

// @brief Get the size of struct which is match the protobuf message.
int GetMessageStructSize(const Message &msg)
{
	return GetMessageStructSize(msg.GetDescriptor());
}

// protobuf does not provide generic template function, so we wrap it.
template<typename Ty>
inline void ProtoSet(const Ty &value, Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
}

#define SPECIAL_TEMPLATE_ProtoSet(type, Type) \
template<> \
inline void ProtoSet<type>(const type &value, Message &msg, \
	const Reflection *refl, const FieldDescriptor *field) { \
	return refl->Set ## Type(&msg, field, value); \
}

SPECIAL_TEMPLATE_ProtoSet(int32_t, Int32)
SPECIAL_TEMPLATE_ProtoSet(int64_t, Int64)
SPECIAL_TEMPLATE_ProtoSet(uint32_t, UInt32)
SPECIAL_TEMPLATE_ProtoSet(uint64_t, UInt64)
SPECIAL_TEMPLATE_ProtoSet(float, Float)
SPECIAL_TEMPLATE_ProtoSet(double, Double)
SPECIAL_TEMPLATE_ProtoSet(bool, Bool)
SPECIAL_TEMPLATE_ProtoSet(Enum, EnumValue)
SPECIAL_TEMPLATE_ProtoSet(std::string, String)

template<typename Ty>
inline void ProtoAdd(const Ty &value, Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
}

#define SPECIAL_TEMPLATE_ProtoAdd(type, Type) \
template<> \
inline void ProtoAdd<type>(const type &value, Message &msg, \
	const Reflection *refl, const FieldDescriptor *field) { \
	return refl->Add ## Type(&msg, field, value); \
}

SPECIAL_TEMPLATE_ProtoAdd(int32_t, Int32)
SPECIAL_TEMPLATE_ProtoAdd(int64_t, Int64)
SPECIAL_TEMPLATE_ProtoAdd(uint32_t, UInt32)
SPECIAL_TEMPLATE_ProtoAdd(uint64_t, UInt64)
SPECIAL_TEMPLATE_ProtoAdd(float, Float)
SPECIAL_TEMPLATE_ProtoAdd(double, Double)
SPECIAL_TEMPLATE_ProtoAdd(bool, Bool)
SPECIAL_TEMPLATE_ProtoAdd(Enum, EnumValue)
SPECIAL_TEMPLATE_ProtoAdd(std::string, String)

// end of wrap protobuf SetXxx function

template<typename Ty>
void SetProtoValue(const uint8_t *&bytes, Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	if (field->is_repeated()) {
		auto &values = *(const std::vector<Ty>*)bytes;
		for (const Ty &value : values) {
			ProtoAdd<Ty>(value, msg, refl, field);
		}
		bytes += sizeof(std::vector<Ty>);
	} else {
		ProtoSet<Ty>(*(Ty*)bytes, msg, refl, field);
		bytes += sizeof(Ty);
	}
}

template<typename Ty>
bool SetProtoMap(const uint8_t *&bytes, Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	auto &values = *(std::map<Ty, uint8_t>*)bytes;
	for (auto &pair : values) {
		auto submsg = refl->AddMessage(&msg, field);
		auto data = (const uint8_t *)&pair;
		if (!StructToProtoInternal(data, *submsg)) return false;
	}
	bytes += sizeof(std::map<Ty, uint8_t>);
	return true;
}

static bool SetProtoMap(const uint8_t *&bytes, Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	auto desc = field->message_type();
	if (desc->field_count() != 2) return false; // map have 2 field.
	auto key = desc->field(0);
	switch (key->cpp_type()) {
	case FieldDescriptor::CPPTYPE_INT32:
		return SetProtoMap<int32_t>(bytes, msg, refl, field);
	case FieldDescriptor::CPPTYPE_INT64:
		return SetProtoMap<int64_t>(bytes, msg, refl, field);
	case FieldDescriptor::CPPTYPE_UINT32:
		return SetProtoMap<uint32_t>(bytes, msg, refl, field);
	case FieldDescriptor::CPPTYPE_UINT64:
		return SetProtoMap<uint64_t>(bytes, msg, refl, field);
	case FieldDescriptor::CPPTYPE_STRING:
		return SetProtoMap<std::string>(bytes, msg, refl, field);
	default:
		return false;
	}
}

static bool SetProtoMessage(const uint8_t *&bytes, Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	if (!field->is_repeated()) {
		auto submsg = refl->MutableMessage(&msg, field);
		return StructToProtoInternal(bytes, *submsg);
	}
	if (field->is_map()) {
		return SetProtoMap(bytes, msg, refl, field);
	}
	auto &values = *(std::vector<uint8_t>*)bytes;
	const uint8_t *data = values.data();
	int size = GetMessageStructSize(field->message_type());
	if (size == 0) return false;
	if (values.size() % size) {
		// The element in vector has difference size?
		return false; // should never reached!
	}
	int count = (int)values.size() / size;
	for (int j = 0; j < count; ++j) {
		auto submsg = refl->AddMessage(&msg, field);
		if (!StructToProtoInternal(data, *submsg)) return false;
	}
	bytes += sizeof(std::vector<uint8_t>);
	return true;
}

// @brief convert struct to protobuf message.
// @param bytes struct memory buffer
// @param msg   protobuf message
bool StructToProtoInternal(const uint8_t *&bytes, Message &msg)
{
	auto refl = msg.GetReflection();
	auto desc = msg.GetDescriptor();
	for (int i = 0; i < desc->field_count(); ++i) {
		auto field = desc->field(i);
		switch (field->cpp_type()) {
		case FieldDescriptor::CPPTYPE_INT32:
			SetProtoValue<int32_t>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_INT64:
			SetProtoValue<int64_t>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_UINT32:
			SetProtoValue<uint32_t>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_UINT64:
			SetProtoValue<uint64_t>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_DOUBLE:
			SetProtoValue<double>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_FLOAT:
			SetProtoValue<float>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_BOOL:
			SetProtoValue<bool>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_ENUM:
			SetProtoValue<Enum>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_STRING:
			SetProtoValue<std::string>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:
			if (!SetProtoMessage(bytes, msg, refl, field)) return false;
			break;
		default:
			return false; // never reached!
		}
	}
	return true;
}

// Construct Object, depends on the implementation of STL.
template<typename Ty>
Ty &InitObject(uint8_t *bytes)
{
	uint64_t *buff = (uint64_t*)new Ty();
	uint64_t begin = (uint64_t)buff, end = begin + sizeof(Ty);
	for (int i = 0; i < (int)(sizeof(Ty) / sizeof(uint64_t)); ++i) {
		if (begin <= buff[i] && buff[i] <= end) {
			((uint64_t*)bytes)[i] = (uint64_t)bytes + buff[i] - begin;
		} else {
			((uint64_t*)bytes)[i] = buff[i];
		}
	}
	delete buff;
	return *(Ty*)bytes;
}

template<typename Ty>
inline Ty &ConvertTo(uint8_t *bytes)
{
	if (std::is_trivial<Ty>::value) return *(Ty*)bytes;
	// if the object is not constructed, construct it;
	bool needinit = true;
	for (int i = 0; i < (int)sizeof(Ty) && needinit; i++) {
		if (bytes[i] != 0) needinit = false;
	}
	if (needinit) InitObject<Ty>(bytes);
	return *(Ty*)bytes;
}

// protobuf does not provide generic template function, so we wrap it.
template<typename Ty>
inline Ty ProtoGet(const Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
}

#define SPECIAL_TEMPLATE_ProtoGet(type, Type) \
template<> \
inline type ProtoGet<type>(const Message &msg, \
	const Reflection *refl, const FieldDescriptor *field) { \
	return refl->Get ## Type(msg, field); \
}

SPECIAL_TEMPLATE_ProtoGet(int32_t, Int32)
SPECIAL_TEMPLATE_ProtoGet(int64_t, Int64)
SPECIAL_TEMPLATE_ProtoGet(uint32_t, UInt32)
SPECIAL_TEMPLATE_ProtoGet(uint64_t, UInt64)
SPECIAL_TEMPLATE_ProtoGet(float, Float)
SPECIAL_TEMPLATE_ProtoGet(double, Double)
SPECIAL_TEMPLATE_ProtoGet(bool, Bool)
SPECIAL_TEMPLATE_ProtoGet(Enum, EnumValue)
SPECIAL_TEMPLATE_ProtoGet(std::string, String)

template<typename Ty>
inline Ty ProtoGetRepeated(const Message &msg,
	const Reflection *refl, const FieldDescriptor *field, int index)
{
}

#define SPECIAL_TEMPLATE_ProtoGetRepeated(type, Type) \
template<> \
inline type ProtoGetRepeated<type>(const Message &msg, \
	const Reflection *refl, const FieldDescriptor *field, int index) { \
	return refl->GetRepeated ## Type(msg, field, index); \
}

SPECIAL_TEMPLATE_ProtoGetRepeated(int32_t, Int32)
SPECIAL_TEMPLATE_ProtoGetRepeated(int64_t, Int64)
SPECIAL_TEMPLATE_ProtoGetRepeated(uint32_t, UInt32)
SPECIAL_TEMPLATE_ProtoGetRepeated(uint64_t, UInt64)
SPECIAL_TEMPLATE_ProtoGetRepeated(float, Float)
SPECIAL_TEMPLATE_ProtoGetRepeated(double, Double)
SPECIAL_TEMPLATE_ProtoGetRepeated(bool, Bool)
SPECIAL_TEMPLATE_ProtoGetRepeated(Enum, EnumValue)
SPECIAL_TEMPLATE_ProtoGetRepeated(std::string, String)

// end of wrap protobuf GetXxx function

template<typename Ty>
void SetStructValue(uint8_t *&bytes, const Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	if (field->is_repeated()) {
		auto &values = ConvertTo<std::vector<Ty>>(bytes);
		int count = refl->FieldSize(msg, field);
		for (int i = 0; i < count; ++i) {
			values.push_back(ProtoGetRepeated<Ty>(msg, refl, field, i));
		}
		bytes += sizeof(std::vector<Ty>);
	} else {
		auto &value = ConvertTo<Ty>(bytes);
		value = ProtoGet<Ty>(msg, refl, field);
		bytes += sizeof(Ty);
	}
}

template<typename Tk, typename Tv>
bool SetStructMap(uint8_t *&bytes, const Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	auto &map = ConvertTo<std::map<Tk, Tv>>(bytes);
	int count = refl->FieldSize(msg, field);
	for (int i = 0; i < count; ++i) {
		auto &submsg = refl->GetRepeatedMessage(msg, field, i);
		auto subrefl = submsg.GetReflection();
		auto subdesc = submsg.GetDescriptor();
		auto subfield = subdesc->field(0);
		Tk key = ProtoGet<Tk>(submsg, subrefl, subfield);
		auto &value = map[key];
		auto data = (uint8_t*)&value;
		if (!ProtoToStructInternal(data, submsg, 1)) return false;
	}
	bytes += sizeof(std::map<Tk, Tv>);
	return true;
}

template<int N>
bool SetStructMap(uint8_t *&bytes, const Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	auto desc = field->message_type();
	if (desc->field_count() != 2) return false; // map have 2 field.
	struct holder { uint8_t member[N]; };
	auto key = desc->field(0);
	switch (key->cpp_type()) {
	case FieldDescriptor::CPPTYPE_INT32:
		return SetStructMap<int32_t, holder>(bytes, msg, refl, field);
	case FieldDescriptor::CPPTYPE_INT64:
		return SetStructMap<int64_t, holder>(bytes, msg, refl, field);
	case FieldDescriptor::CPPTYPE_UINT32:
		return SetStructMap<uint32_t, holder>(bytes, msg, refl, field);
	case FieldDescriptor::CPPTYPE_UINT64:
		return SetStructMap<uint64_t, holder>(bytes, msg, refl, field);
	case FieldDescriptor::CPPTYPE_STRING:
		return SetStructMap<std::string, holder>(bytes, msg, refl, field);
	default:
		return false;
	}
}

static bool SetStructMap(uint8_t *&bytes, size_t size, const Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	size -= sizeof(std::string);
	if (size <= 0x0010) return SetStructMap<0x0010>(bytes, msg, refl, field);
	if (size <= 0x0020) return SetStructMap<0x0020>(bytes, msg, refl, field);
	if (size <= 0x0040) return SetStructMap<0x0040>(bytes, msg, refl, field);
	if (size <= 0x0080) return SetStructMap<0x0080>(bytes, msg, refl, field);
	if (size <= 0x0100) return SetStructMap<0x0100>(bytes, msg, refl, field);
	if (size <= 0x0200) return SetStructMap<0x0200>(bytes, msg, refl, field);
	if (size <= 0x0400) return SetStructMap<0x0400>(bytes, msg, refl, field);
	if (size <= 0x0800) return SetStructMap<0x0800>(bytes, msg, refl, field);
	if (size <= 0x1000) return SetStructMap<0x1000>(bytes, msg, refl, field);
	if (size <= 0x2000) return SetStructMap<0x2000>(bytes, msg, refl, field);
	if (size <= 0x4000) return SetStructMap<0x4000>(bytes, msg, refl, field);
	if (size <= 0x8000) return SetStructMap<0x8000>(bytes, msg, refl, field);
	// The struct is too big, max sizeof struct is 0x8000;
	return false;
}

static bool SetStructMessage(uint8_t *&bytes, const Message &msg,
	const Reflection *refl, const FieldDescriptor *field)
{
	if (!field->is_repeated()) {
		auto &submsg = refl->GetMessage(msg, field);
		return ProtoToStructInternal(bytes, submsg, 0);
	}
	int size = GetMessageStructSize(field->message_type());
	if (field->is_map()) {
		return SetStructMap(bytes, size, msg, refl, field);
	}
	// deal repeated message as vector
	auto &values = *(std::vector<uint8_t>*)bytes;
	int count = refl->FieldSize(msg, field);
	values.resize(count * size);
	auto data = (uint8_t*)values.data();
	for (int j = 0; j < count; ++j) {
		auto &submsg = refl->GetRepeatedMessage(msg, field, j);
		if (!ProtoToStructInternal(data, submsg, 0)) return false;
	}
	bytes += sizeof(std::vector<uint8_t>);
	return true;
}

// @brief convert protobuf message to struct.
// @param bytes: struct memory buffer
// @param msg  : protobuf message
// @param start: if msg is map k-v pair, set start 1, else set it 0
bool ProtoToStructInternal(uint8_t *&bytes, const Message &msg, int start)
{
	auto refl = msg.GetReflection();
	auto desc = msg.GetDescriptor();
	for (int i = start; i < desc->field_count(); ++i) {
		auto field = desc->field(i);
		switch (field->cpp_type()) {
		case FieldDescriptor::CPPTYPE_INT32:
			SetStructValue<int32_t>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_INT64:
			SetStructValue<int64_t>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_UINT32:
			SetStructValue<uint32_t>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_UINT64:
			SetStructValue<uint64_t>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_DOUBLE:
			SetStructValue<double>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_FLOAT:
			SetStructValue<float>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_BOOL:
			SetStructValue<bool>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_ENUM:
			SetStructValue<Enum>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_STRING:
			SetStructValue<std::string>(bytes, msg, refl, field);
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:
			if (!SetStructMessage(bytes, msg, refl, field)) return false;
			break;
		default:
			return false; // never reached!
		}
	}
	return true;
}

} // namespace cps

