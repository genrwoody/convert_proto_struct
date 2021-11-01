#pragma once

#include <vector>
#include <string>
#include <map>

enum Enum
{
	EnFlag1,
	EnFlag2
};

struct Message1
{
	int32_t member1;
	int64_t member2;
	uint32_t member3;
	uint64_t member4;
	Enum member5;
	bool member6;
	std::string member7;
	std::map<std::string, int32_t> member8;
	std::vector<int32_t> member9;
};

struct Message2
{
	struct Message3
	{
		std::map<std::string, int64_t> member1;
		std::vector<int64_t> member2;
		std::map<int64_t, std::string> member3;
	};
	std::map<std::string, Message3> member1;
	std::vector<Message1> member2;
	double member3;
	float member4;
	Message1 member5;
	Message3 member6;
};

