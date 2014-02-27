#include "tweetop.h"
#include <ctime>

using namespace std;

namespace TweetOp {

bool userExist(NaiveDB *db, const char *user) {
	DBData user_d;
	user_d.type = DBType::STRING;
	user_d.str = user;
	vector<RecordHandle> query_res = db->query("userinfo","user",user_d);
	// returns true even when user is deleted
	// it's not a bug
	return query_res.empty();
}

int64_t login(NaiveDB *db, const char *user, const char *passwd) {
	vector<RecordHandle> query_res;
	DBData user_d(DBType::STRING);
	user_d.str = user;
	query_res = db->query("userinfo","user",user_d);

	if (query_res.empty()) {
		return -1; // user not found
	}
	if (db->get(query_res[0],"deleted").boolean == true)
		return -1; // user deleted

	string correct_passwd = db->get(query_res[0],"passwd").str;
	if (strcmp(correct_passwd.c_str(),passwd) != 0)
		return 0; // incorrect password
	// success login
	DBData uid_d = db->get(query_res[0],"id");
	return uid_d.int64;
}

void registerAccount(NaiveDB *db, const char *user,
					 const char *passwd,
					 const char *birthday,
					 const char *name,
					 const char *gender,
					 const char *intro) {

	vector<DBData> line;
	DBData user_d(DBType::STRING), passwd_d(DBType::STRING),
			birthday_d(DBType::STRING), name_d(DBType::STRING),
			gender_d(DBType::BOOLEAN), intro_d(DBType::STRING);
	user_d.str = user;
	line.push_back(user_d);
	passwd_d.str = passwd;
	line.push_back(passwd_d);
	birthday_d.str = birthday;
	line.push_back(birthday_d);
	name_d.str = name;
	line.push_back(name_d);
	gender_d.boolean = strcmp(gender,"M") == 0? true:false;
	line.push_back(gender_d);
	intro_d.str = intro;
	line.push_back(intro_d);
	db->insert("userinfo",line);
}

void follow(NaiveDB *db, int64_t uid, int64_t id) {
	DBData uid_d(DBType::INT64);
	uid_d.int64 = uid;
	vector<RecordHandle> query_res = db->query("afob","a",uid_d);
	bool deleted = false;
	for (RecordHandle handle : query_res) {
		if (db->get(handle,"b").int64 == id) {
			deleted = true;
			DBData tmp(DBType::BOOLEAN);
			tmp.boolean = false;
			db->modify(handle,"deleted",tmp);
			break;
		}
	}

	if (!deleted) {
		vector<DBData> line;
		DBData dbd(DBType::INT64);
		dbd.int64 = uid;
		line.push_back(dbd); // a
		dbd.int64 = id;
		line.push_back(dbd); // b
		dbd.type = DBType::BOOLEAN;
		dbd.boolean = false;
		line.push_back(dbd); // deleted
		db->insert("afob",line);
	}
}

void unfollow(NaiveDB *db, int64_t uid, int64_t id) {
	DBData uid_d(DBType::INT64);
	uid_d.int64 = uid;
	vector<RecordHandle> query_res = db->query("afob","a",uid_d);
	for (RecordHandle handle : query_res) {
		if (db->get(handle,"b").int64 == id) {
			DBData tmp(DBType::BOOLEAN);
			tmp.boolean = true;
			db->modify(handle,"deleted",tmp);
			break;
		}
	}
}

void retweet(NaiveDB *db, int64_t uid, const TweetLine &tweet) {
	int32_t unix_time = time(0);
	vector<DBData> dbline;
	DBData dbd(DBType::STRING);
	dbd.str = tweet.content;
	dbline.push_back(dbd); // content
	dbd.type = DBType::INT64;
	dbd.int64 = uid;
	dbline.push_back(dbd); // publisher
	dbd.int64 = tweet.author;
	dbline.push_back(dbd); // author
	dbd.type = DBType::INT32;
	dbd.int32 = unix_time;
	dbline.push_back(dbd); // time
	dbd.type = DBType::BOOLEAN;
	dbd.boolean = false;
	dbline.push_back(dbd); // deleted

	db->insert("tweets",dbline);
}

void newTweet(NaiveDB *db, int64_t uid, const char *content) {
	int32_t unix_time = time(0);
	vector<DBData> dbline;
	DBData dbd(DBType::STRING);
	dbd.str = content;
	dbline.push_back(dbd); // content
	dbd.type = DBType::INT64;
	dbd.int64 = uid;
	dbline.push_back(dbd); // publisher
	dbline.push_back(dbd); // author
	dbd.type = DBType::INT32;
	dbd.int32 = unix_time;
	dbline.push_back(dbd); // time
	dbd.type = DBType::BOOLEAN;
	dbd.boolean = false;
	dbline.push_back(dbd); // deleted

	db->insert("tweets",dbline);
}

}
