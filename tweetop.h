#ifndef TWEETOP_H
#define TWEETOP_H

#include <string>
#include <vector>
#include "naivedb.h"

class TweetLine {
public:
	int64_t publisher;
	int64_t author;
	std::string content;
	int32_t time;

	TweetLine(const std::string &content,int64_t publisher, int64_t author,int32_t time) {
		this->content = content;
		this->publisher = publisher;
		this->author = author;
		this->time = time;
	}
};

inline bool operator<(const TweetLine &lval,const TweetLine &rval) {
	return lval.time >= rval.time;
}

class UserLine {
public:
	int64_t uid;
	std::string username;
	std::string birthday;
	std::string name;
	bool male;
	std::string intro;
};

#endif // TWEETOP_H
