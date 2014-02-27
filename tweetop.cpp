#include "tweetop.h"

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

}
