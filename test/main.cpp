#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include "message.h"
#include "message.pb.h"
#include "convert_proto_struct.h"

int main()
{
	Message2 msg2;
	Message2::Message3 msg3;
	msg3.member1.emplace("msg3mem1key1", 1234567890);
	msg3.member1.emplace("msg3mem1key2", 345645674567);
	msg3.member1.emplace("msg3mem1key3", 98798768765);
	msg3.member2.emplace_back(6783443);
	msg3.member2.emplace_back(8763456);
	msg3.member3.emplace(123, "msg3mem3value1");
	msg3.member3.emplace(456, "msg3mem3value2");
	msg2.member1.emplace("msg2mem1key1", msg3);
	msg2.member1.emplace("msg2mem1key2", msg3);
	msg2.member1.emplace("msg2mem1key3", msg3);
	Message1 msg1;
	msg1.member1 = -12345;
	msg1.member2 = -87654321987654321l;
	msg1.member3 = 12345;
	msg1.member4 = 7654321987654321l;
	msg1.member5 = Enum::EnFlag1;
	msg1.member6 = true;
	msg1.member7 = "msg1member7";
	msg1.member8.emplace("msg1mefiem8key1", 123456);
	msg1.member8.emplace("msg1mem8key2", 654321);
	msg1.member9.emplace_back(898989);
	msg1.member9.emplace_back(767676);
	msg2.member2.emplace_back(msg1);
	msg2.member2.emplace_back(msg1);
	msg2.member3 = 3.1415926;
	msg2.member4 = 1.4142135f;

	proto::Message2 proto_msg;
	if (!cps::StructToProto(msg2, proto_msg)) {
		printf("struct to proto failed.\n");
		return -1;
	}
	proto_msg.PrintDebugString();
	Message2 struct_msg;
	if (!cps::ProtoToStruct(proto_msg, struct_msg)) {
		printf("proto to struct failed.\n");
		return -1;
	}
	if (!(struct_msg == msg2)) {
		printf("proto to struct failed.\n");
	}
	printf("test success!\n");
	return 0;
}

