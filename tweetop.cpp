#include "tweetop.h"

using namespace std;

namespace TweetOp {

bool userExist(NaiveDB *db, const char *user) {
	DBData query_dat;
	query_dat.type = DBType::STRING;
	query_dat.str = user;
	vector<RecordHandle> query_res = db->query("userinfo","user",query_dat);
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

}
