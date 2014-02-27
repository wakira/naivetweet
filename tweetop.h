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

// userExist(...) note that it returns true even when user is deleted
// it's not a bug
bool userExist(NaiveDB *db, const char *user);

// login(...) returns uid when success, -1 when user is not found, 0 when password is incorrect
int64_t login(NaiveDB *db, const char *user, const char *passwd);

// registerAccount(...)
// call this function after confirming that user does not exist
// be careful about these arguments
void registerAccount(NaiveDB *db,const char *user,
					 const char *passwd,
					 const char *birthday,
					 const char *name,
					 const char *gender,
					 const char *intro);

// follow(...) make uid follow id
void follow(NaiveDB *db, int64_t uid, int64_t id);

// unfollow(...) make uid unfollow id
void unfollow(NaiveDB *db, int64_t uid, int64_t id);

// retweet(...) retweet helper
void retweet(NaiveDB *db, int64_t uid, const TweetLine &tweet);

}

#endif // TWEETOP_H
