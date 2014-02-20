#ifndef SPSTRING_HPP
#define SPSTRING_HPP

#include <string>

struct SPString
{
	SPString(size_t len, const std::string &source) :
		length(len), str(source) {}
	size_t length;
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
