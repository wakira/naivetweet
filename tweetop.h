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

namespace TweetOp {

bool userExist(NaiveDB *db, const char *user);

}

#endif // TWEETOP_H
