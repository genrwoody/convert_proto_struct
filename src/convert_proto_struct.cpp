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

typedef std::vector<uint8_t> Vector;
typedef std::map<uint8_t, uint8_t> Map;

// for protobuf enum, different from int
struct Enum
{
	int value;
	Enum() : value(0) {}
	Enum(int v) : value(v) {}
	operator int() const { return value; }
};

// calculate and storage the struct size and alignment.
class StructInfo
{
protected:
	int _size;  // sizeof(struct)
	int _align; // alignof(struct)
public:
	StructInfo(int size = 0, int align = 0)
		: _size(size), _align(align) {}
	// construct with protobuf message descriptor.
	StructInfo(const Descriptor *desc);
	// get size;
	inline int size() const { return _size; }
	// get alignment
	inline int align() const { return _align; }
protected:
	// append member, for calculate struct size and align.
	inline void append(const StructInfo &info)
	{
		if (_align < info._align) {
			_align = info._align;
		}
		int remain = _align - _size % _align;
		_size += remain % info._align + info._size;
	}
	// append member, for calculate struct size and align.
	template<typename Ty>
	inline void append()
	{
		append(StructInfo{ sizeof(Ty), alignof(Ty) });
	}
};

StructInfo::StructInfo(const Descriptor *desc)
{
	_size = _align = 0;
	for (int i = 0; i < desc->field_count(); ++i) {
		auto field = desc->field(i);
		if (field->is_map()) {
			append<Map>();
			continue;
		}
		if (field->is_repeated()) {
			append<Vector>();
			continue;
		}
		switch (field->cpp_type()) {
		case FieldDescriptor::CPPTYPE_INT32:
		case FieldDescriptor::CPPTYPE_UINT32:
		case FieldDescriptor::CPPTYPE_FLOAT:
		case FieldDescriptor::CPPTYPE_ENUM:
			append<int32_t>();
			break;
		case FieldDescriptor::CPPTYPE_INT64:
		case FieldDescriptor::CPPTYPE_UINT64:
		case FieldDescriptor::CPPTYPE_DOUBLE:
			append<int64_t>();
			break;
		case FieldDescriptor::CPPTYPE_BOOL:
			append<bool>();
			break;
		case FieldDescriptor::CPPTYPE_STRING:
			append<std::string>();
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:
		{
			append(field->message_type());
			break;
		}
		default:
			_size = _align = 0;
			return; // never reached!
		}
	}
}

// ==================== convert struct to protobuf message ====================

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

// read member value from struct and set to protobuf message.
class  StructReader : public StructInfo
{
private:
	int _pos; // read position
	const uint8_t *_bytes; // the memory of struct
	Message &_msg; // protobuf message
	const Reflection *_refl; // protobuf message reflection
public:
	StructReader(Message &msg, const uint8_t *bytes)
		: StructInfo(msg.GetDescriptor())
		, _pos(0)
		, _bytes(bytes)
		, _msg(msg)
		, _refl(msg.GetReflection()) {}

	// convert to protobuf message
	bool to_proto();

private:
	// read a member from struct
	template<typename Ty>
	const Ty &read_member()
	{
		int remain = _align - _pos % _align;
		_pos += remain % alignof(Ty);
		auto &result = *(const Ty*)(_bytes + _pos);
		_pos += sizeof(Ty);
		return result;
	}

	// read a struct member from struct
	StructReader child_reader(Message &msg)
	{
		StructReader reader(msg, nullptr);
		int remain = _align - _pos % _align;
		_pos += remain % reader._align;
		reader._bytes = _bytes + _pos;
		_pos += reader._size;
		return reader;
	}

	// set value to protobuf message
	template<typename Ty>
	bool set_proto_value(const FieldDescriptor *field)
	{
		if (field->is_repeated()) {
			auto &values = read_member<std::vector<Ty>>();
			for (const Ty &value : values) {
				ProtoAdd<Ty>(value, _msg, _refl, field);
			}
		} else {
			ProtoSet<Ty>(read_member<Ty>(), _msg, _refl, field);
		}
		return true;
	}

	// set protobuf map message
	template<typename Ty>
	bool set_proto_map(const FieldDescriptor *field)
	{
		auto &values = read_member<std::map<Ty, Ty>>();
		for (auto &pair : values) {
			auto submsg = _refl->AddMessage(&_msg, field);
			auto bytes = (const uint8_t*)&pair;
			StructReader reader(*submsg, bytes);
			if (!reader.to_proto()) {
				return false;
			}
		}
		return true;
	}

	// set protpbuf map message, forward to next function by alignment.
	bool set_proto_map(const FieldDescriptor *field)
	{
		auto desc = field->message_type();
		if (desc->field_count() != 2) {
			// map should have 2 field.
			return false; // should never reached!
		}
		StructInfo info(desc);
		switch (info.align()) {
		case 4:
			return set_proto_map<int32_t>(field);
		case 8:
			return set_proto_map<int64_t>(field);
		default:
			// protobuf support (u)int32/(u)int64/string as key,
			// the key is align as 4 or 8 bytes.
			return false; // should never reached!
		}
	}
};

// set protobuf message value
template<>
bool StructReader::set_proto_value<Message>(const FieldDescriptor *field)
{
	if (!field->is_repeated()) {
		auto submsg = _refl->MutableMessage(&_msg, field);
		auto reader = child_reader(*submsg);
		return reader.to_proto();
	}
	if (field->is_map()) {
		return set_proto_map(field);
	}
	// set protobuf repeated message from vector
	StructInfo info(field->message_type());
	if (info.size() == 0) {
		// an empty struct in vector?
		return false; // should never reached!
	}
	auto &values = read_member<Vector>();
	const uint8_t *data = values.data();
	if (values.size() % info.size()) {
		// The element in vector has difference size?
		return false; // should never reached!
	}
	int count = static_cast<int>(values.size() / info.size());
	for (int i = 0; i < count; ++i) {
		auto submsg = _refl->AddMessage(&_msg, field);
		StructReader reader(*submsg, data);
		if (!reader.to_proto()) {
			return false;
		}
		data += info.size(); // point to next member
	}
	return true;
}

bool StructReader::to_proto()
{
	auto desc = _msg.GetDescriptor();
	for (int i = 0; i < desc->field_count(); ++i) {
		auto field = desc->field(i);
		switch (field->cpp_type()) {
		case FieldDescriptor::CPPTYPE_INT32:
			set_proto_value<int32_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_INT64:
			set_proto_value<int64_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_UINT32:
			set_proto_value<uint32_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_UINT64:
			set_proto_value<uint64_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_DOUBLE:
			set_proto_value<double>(field);
			break;
		case FieldDescriptor::CPPTYPE_FLOAT:
			set_proto_value<float>(field);
			break;
		case FieldDescriptor::CPPTYPE_BOOL:
			set_proto_value<bool>(field);
			break;
		case FieldDescriptor::CPPTYPE_ENUM:
			set_proto_value<Enum>(field);
			break;
		case FieldDescriptor::CPPTYPE_STRING:
			set_proto_value<std::string>(field);
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:
			if (!set_proto_value<Message>(field)) {
				return false;
			}
			break;
		default:
			return false; // never reached!
		}
	}
	return true;
}

// @brief Convert struct to protobuf message.
// @param[in] bytes: pointer to struct
// @param[in] size: sizeof struct
// @param[out] msg: protobuf message
// @return true for success, or false for failed.
bool StructToProto(const void *bytes, size_t size, Message &msg)
{
	StructReader reader(msg, static_cast<const uint8_t*>(bytes));
	if (reader.size() != static_cast<int>(size)) {
		return false; // protobuf message is not match struct
	}
	return reader.to_proto();
}

// ==================== convert protobuf message to struct ====================

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
inline Ty ProtoGet(const Message &msg,
	const Reflection *refl, const FieldDescriptor *field, int index)
{
}

#define SPECIAL_TEMPLATE_ProtoGetRepeated(type, Type) \
template<> \
inline type ProtoGet<type>(const Message &msg, \
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

// get member value from protobuf message and write to struct.
class StructWriter : public StructInfo
{
private:
	bool _placement; // need placement new
	int _pos; // read position
	uint8_t *_bytes; // the memory of struct
	const Message &_msg; // protobuf message
	const Reflection *_refl; // protobuf message reflection
public:
	StructWriter(const Message &msg, uint8_t *bytes)
		: StructInfo(msg.GetDescriptor())
		, _placement(false)
		, _pos(0)
		, _bytes(bytes)
		, _msg(msg)
		, _refl(msg.GetReflection()) {}
	// convert from protobuf message
	bool from_proto();
private:
	// read a member from struct
	template<typename Ty>
	Ty &read_member()
	{
		int remain = _align - _pos % _align;
		_pos += remain % alignof(Ty);
		uint8_t *data = _bytes + _pos;
		_pos += sizeof(Ty);
		Ty *result = _placement ? new(data)Ty : (Ty*)data;
		return *result;
	}

	// read a struct member from struct
	StructWriter child_writer(const Message &msg)
	{
		StructWriter writer(msg, nullptr);
		int remain = _align - _pos % _align;
		_pos += remain % writer._align;
		writer._bytes = _bytes + _pos;
		_pos += writer._size;
		writer._placement = _placement;
		return writer;
	}

	// set member value from protobuf message
	template<typename Ty>
	bool set_struct_value(const FieldDescriptor *field)
	{
		if (field->is_repeated()) {
			auto &values = read_member<std::vector<Ty>>();
			int count = _refl->FieldSize(_msg, field);
			for (int i = 0; i < count; ++i) {
				Ty value = ProtoGet<Ty>(_msg, _refl, field, i);
				values.push_back(value);
			}
		} else {
			auto &value = read_member<Ty>();
			value = ProtoGet<Ty>(_msg, _refl, field);
		}
		return true;
	}

	// set struct map member with protobuf message.
	bool set_struct_map(const FieldDescriptor *field);

	template<int ValueSize, int Alignment>
	inline bool set_struct_map(const FieldDescriptor *field);

	template<typename Key, typename Value>
	bool set_struct_map(const FieldDescriptor *field);
};

template<typename Key, typename Value>
bool StructWriter::set_struct_map(const FieldDescriptor *field)
{
	auto &map = read_member<std::map<Key, Value>>();
	int count = _refl->FieldSize(_msg, field);
	for (int i = 0; i < count; ++i) {
		auto &submsg = _refl->GetRepeatedMessage(_msg, field, i);
		auto subrefl = submsg.GetReflection();
		auto subdesc = submsg.GetDescriptor();
		auto subfield = subdesc->field(0);
		auto key = ProtoGet<Key>(submsg, subrefl, subfield);
		auto pair = map.emplace(key, Value{ 0 });
		if (!pair.second) {
			return false;
		}
		auto data = (uint8_t*)&(pair.first->first);
		StructWriter writer(submsg, data);
		writer._placement = true;
		if (!writer.from_proto()) {
			return false;
		}
	}
	return true;
}

template<int ValueSize, int Alignment>
struct ValueType {};

template<int ValueSize>
struct ValueType<ValueSize, 4>
{
	uint32_t member[ValueSize / 4];
};

template<int ValueSize>
struct ValueType<ValueSize, 8>
{
	uint64_t member[ValueSize / 8];
};

template<int ValueSize, int Alignment>
bool StructWriter::set_struct_map(const FieldDescriptor *field)
{
	typedef ValueType<ValueSize, Alignment> Value;
	auto desc = field->message_type();
	auto key = desc->field(0);
	switch (key->cpp_type()) {
	case FieldDescriptor::CPPTYPE_INT32:
	case FieldDescriptor::CPPTYPE_ENUM:
		return set_struct_map<int32_t, Value>(field);
	case FieldDescriptor::CPPTYPE_INT64:
		return set_struct_map<int64_t, Value>(field);
	case FieldDescriptor::CPPTYPE_UINT32:
		return set_struct_map<uint32_t, Value>(field);
	case FieldDescriptor::CPPTYPE_UINT64:
		return set_struct_map<uint64_t, Value>(field);
	case FieldDescriptor::CPPTYPE_STRING:
		return set_struct_map<std::string, Value>(field);
	default:
		// protobuf support (u)int32/(u)int64/string as key,
		// the key is align as 4 or 8 bytes.
		return false; // should never reached!
	}
}

bool StructWriter::set_struct_map(const FieldDescriptor *field)
{
	auto desc = field->message_type();
	if (desc->field_count() != 2) {
		// map should have 2 field.
		return false; // should never reached!
	}
	StructInfo info(desc); // info is std::pair<key, value>
	auto key = desc->field(0);
	int size = 0;
	switch (key->cpp_type()) {
	case FieldDescriptor::CPPTYPE_INT32:
	case FieldDescriptor::CPPTYPE_ENUM:
	case FieldDescriptor::CPPTYPE_UINT32:
		size = info.size() - sizeof(int32_t);
		break;
	case FieldDescriptor::CPPTYPE_INT64:
	case FieldDescriptor::CPPTYPE_UINT64:
		size = info.size() - sizeof(int64_t);
		break;
	case FieldDescriptor::CPPTYPE_STRING:
		size = info.size() - sizeof(std::string);
		break;
	default:
		// protobuf support (u)int32/(u)int64/string as key,
		return false; // should never reached!
	}
	switch (info.align()) {
	case 4:
		if (size <= 0x008) return set_struct_map<0x008, 4>(field);
		if (size <= 0x010) return set_struct_map<0x010, 4>(field);
		if (size <= 0x020) return set_struct_map<0x020, 4>(field);
		if (size <= 0x040) return set_struct_map<0x040, 4>(field);
		if (size <= 0x080) return set_struct_map<0x080, 4>(field);
		if (size <= 0x100) return set_struct_map<0x100, 4>(field);
		if (size <= 0x200) return set_struct_map<0x200, 4>(field);
		if (size <= 0x400) return set_struct_map<0x400, 4>(field);
		if (size <= 0x800) return set_struct_map<0x800, 4>(field);
		break;
	case 8:
		if (size <= 0x008) return set_struct_map<0x008, 8>(field);
		if (size <= 0x010) return set_struct_map<0x010, 8>(field);
		if (size <= 0x020) return set_struct_map<0x020, 8>(field);
		if (size <= 0x040) return set_struct_map<0x040, 8>(field);
		if (size <= 0x080) return set_struct_map<0x080, 8>(field);
		if (size <= 0x100) return set_struct_map<0x100, 8>(field);
		if (size <= 0x200) return set_struct_map<0x200, 8>(field);
		if (size <= 0x400) return set_struct_map<0x400, 8>(field);
		if (size <= 0x800) return set_struct_map<0x800, 8>(field);
		break;
	default:
		// the alignof map pair is 4 or 8 bytes.
		return false; // should never reached!
	}
	// The struct is too big, max sizeof struct is 0x800;
	return false;
}

template<>
bool StructWriter::set_struct_value<Message>(const FieldDescriptor *field)
{
	if (!field->is_repeated()) {
		auto &submsg = _refl->GetMessage(_msg, field);
		auto writer = child_writer(submsg);
		return writer.from_proto();
	}
	if (field->is_map()) {
		return set_struct_map(field);
	}
	// deal repeated message as vector
	StructInfo info(field->message_type());
	if (info.size() == 0) {
		// an empty struct in vector?
		return false; // should never reached!
	}
	auto &values = read_member<Vector>();
	int count = _refl->FieldSize(_msg, field);
	values.resize(static_cast<size_t>(count) * info.size());
	auto data = values.data();
	for (int i = 0; i < count; ++i) {
		auto &submsg = _refl->GetRepeatedMessage(_msg, field, i);
		StructWriter writer(submsg, data);
		writer._placement = true;
		if (!writer.from_proto()) {
			return false;
		}
		data += info.size(); // point to next member
	}
	return true;
}

bool StructWriter::from_proto()
{
	auto desc = _msg.GetDescriptor();
	for (int i = 0; i < desc->field_count(); ++i) {
		auto field = desc->field(i);
		switch (field->cpp_type()) {
		case FieldDescriptor::CPPTYPE_INT32:
			set_struct_value<int32_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_INT64:
			set_struct_value<int64_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_UINT32:
			set_struct_value<uint32_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_UINT64:
			set_struct_value<uint64_t>(field);
			break;
		case FieldDescriptor::CPPTYPE_DOUBLE:
			set_struct_value<double>(field);
			break;
		case FieldDescriptor::CPPTYPE_FLOAT:
			set_struct_value<float>(field);
			break;
		case FieldDescriptor::CPPTYPE_BOOL:
			set_struct_value<bool>(field);
			break;
		case FieldDescriptor::CPPTYPE_ENUM:
			set_struct_value<Enum>(field);
			break;
		case FieldDescriptor::CPPTYPE_STRING:
			set_struct_value<std::string>(field);
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:
			if (!set_struct_value<Message>(field)) {
				return false;
			}
			break;
		default:
			return false; // never reached!
		}
	}
	return true;
}

// @brief Convert protobuf message to struct.
// @param[in] msg: protobuf message
// @param[out] bytes: pointer to struct
// @param[in] size: sizeof struct
// @return true for success, or false for failed.
bool ProtoToStruct(const Message &msg, void *bytes, size_t size)
{
	StructWriter writer(msg, static_cast<uint8_t*>(bytes));
	if (writer.size() != static_cast<int>(size)) {
		return false; // protobuf message is not match struct
	}
	return writer.from_proto();
}

} // namespace cps

