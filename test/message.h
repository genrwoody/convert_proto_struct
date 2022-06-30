#pragma once

#include <cmath>
#include <vector>
#include <string>
#include <map>

enum Enum : int
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
	bool member10;

	bool operator==(const Message1 &rh) const
	{
		return (
			member1 == rh.member1 &&
			member2 == rh.member2 &&
			member3 == rh.member3 &&
			member4 == rh.member4 &&
			member5 == rh.member5 &&
			member6 == rh.member6 &&
			member7 == rh.member7 &&
			member8 == rh.member8 &&
			member9 == rh.member9 &&
			member10 == rh.member10
		);
	}
};

struct Message2
{
	struct Message3
	{
		std::map<std::string, int64_t> member1;
		std::vector<int64_t> member2;
		std::map<int64_t, std::string> member3;

		bool operator==(const Message3 &rh) const
		{
			return (
				member1 == rh.member1 &&
				member2 == rh.member2 &&
				member3 == rh.member3
			);
		}
	};
	std::map<std::string, Message3> member1;
	std::vector<Message1> member2;
	double member3;
	float member4;
	Message1 member5;
	Message3 member6;
	std::map<int32_t, bool> member7;

	bool operator==(const Message2 &rh) const
	{
		return (
			member1 == rh.member1 &&
			member2 == rh.member2 &&
			std::abs(member3 - rh.member3) < 1e-5 &&
			std::abs(member4 - rh.member4) < 1e-5 &&
			member5 == rh.member5 &&
			member6 == rh.member6 &&
			member7 == rh.member7
		);
	}
};

