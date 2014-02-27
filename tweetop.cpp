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

}
