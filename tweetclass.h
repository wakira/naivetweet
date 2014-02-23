#ifndef TWEETCLASS_H
#define TWEETCLASS_H

#include <string>
#include <vector>

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

#endif // TWEETCLASS_H
