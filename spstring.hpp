#ifndef SPSTRING_HPP
#define SPSTRING_HPP

#include <string>

struct SPString
{
	SPString(int len, const std::string &source) :
		length(len), str(source) {}
	int length;
	std::string str;

	bool operator==(const SPString &rval) {
		return str == rval.str;
	}

	bool operator!=(const SPString &rval) {
		return str != rval.str;
	}

	bool operator<(const SPString &rval) {
		return str < rval.str;
	}

	bool operator<=(const SPString &rval) {
		return str <= rval.str;
	}

	bool operator>=(const SPString &rval) {
		return str >= rval.str;
	}

	bool operator>(const SPString &rval) {
		return str > rval.str;
	}
};

#endif // SPSTRING_H
