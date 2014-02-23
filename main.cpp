#include <algorithm>
#include <string>
#include <cstring>
#include <ctime>
#include <clocale>
#include <cctype>
#include <ncurses.h>
#include "naivedb.h"
#include "tweetclass.h"

using namespace std;

const size_t kMaxLine = 80;

void welcome();
void login();
void registerAccount();
void home();
void homeCommand();
void newTweet();
void viewTweets();
void tweetPageView(const vector<TweetLine> &alltweets);
void changeProfile();

/* Helper functions */
void inputUntilCorrect(const char *prompt, char *input, bool(*test)(char*), const char *failprompt);
bool validUsername(char *user);
bool validPasswd(char *passwd);
bool validBirthday(char *birthday);
bool validName(char *name);
bool validGender(char *raw);
bool validIntro(char *line);
bool validTweet(char *tweet);

class tweetCmp {
public:
	bool operator()(const pair<int64_t,string> &v1, const pair<int64_t,string> &v2) const {
		return v1.first >= v2.first;
	}
};

/* Global variables */
NaiveDB *db;
int64_t uid;

void debug() {
	BPTree<long,long> bp("test.idx");
	for (long i = 1; i <= 20; ++i)
		bp.insert(47,i);
	vector<long> found = bp.find(47);
	for (long x : found)
		printf("%d ",x);
	printf("\n");
}

int main()
{
	setlocale(LC_ALL,"");
	initscr();
	cbreak();

	db = new NaiveDB("db.xml");

	welcome();

	endwin();
	return 0;
}

void changeProfile() {

}

void newTweet() {
	clear();
	echo();
	char buf[500];
	inputUntilCorrect("Enter your tweet:(less than 140 characters)\n",
					  buf, validTweet, "Too long!");
	int32_t unix_time = time(0);
	vector<DBData> dbline;
	DBData dbd(DBType::STRING);
	dbd.str = buf;
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

	home();
}

void viewTweets() {
	vector<TweetLine> alltweets;
	DBData uid_d(DBType::INT64);
	uid_d.int64 = uid;
	vector<RecordHandle> query_res = db->query("afob","a",uid_d);
	vector<int64_t> following_list;
	for (RecordHandle x : query_res) {
		bool deleted = db->get(x,"deleted").boolean;
		if (!deleted)
			following_list.push_back(db->get(x,"b").int64);
	}
	following_list.push_back(uid);
	for (int64_t publisher : following_list) {
		DBData publisher_d(DBType::INT64);
		publisher_d.int64 = publisher;
		vector<RecordHandle> tweets_handle = db->query("tweets","publisher",publisher_d);
		for (RecordHandle x : tweets_handle) {
			bool deleted = db->get(x,"deleted").boolean;
			if (!deleted) {
				int32_t time = db->get(x,"time").int32;
				int64_t author = db->get(x,"author").int64;
				string content = db->get(x,"content").str;
				alltweets.push_back(TweetLine(content,publisher,author,time));
			}
		}
	}
	// sort tweets by time
	sort(alltweets.begin(),alltweets.end());
	// show these tweets
	tweetPageView(alltweets);
}

void tweetPageView(const vector<TweetLine> &alltweets) {
	// TODO
	for (TweetLine x : alltweets) {
		printw("%d:%s\n",x.publisher,x.content.c_str());
	}
	getch();
	home();
}

void homeCommand() {
GOTO_TAG_homeCommand:
	noecho();
	char key = getch();
	switch (key) {
	case 't': case 'T':
		newTweet();
		break;
	case 'v': case 'V':
		viewTweets();
		break;
	case 'f': case 'F':

		break;
	case 'l': case 'L':
		break;
	case 'r': case 'R':
		changeProfile();
		break;
	case 'x': case 'X':
		break;
	default:
		goto GOTO_TAG_homeCommand;
	}
}

void home() {
	clear();
	printw("[t] to tweet\n");
	printw("[v] to view tweets\n");
	printw("[f] to follow somebody\n");
	printw("[l] to list friends\n");
	printw("[r] to change profile");
	printw("[x] to exit\n");
	homeCommand();
}

void welcome() {
	printw("Welcome to naivetweet, press 'l' to login, 'r' to register, 'q' to quit\n");
	refresh();
	noecho();
	while (true) {
		char c = getch();
		echo();
		if (c == 'l' || c == 'L') {
			login();
			return;
		}
		if (c == 'r' || c == 'R') {
			registerAccount();
			return;
		}
		if (c == 'q' || c == 'Q') {
			return;
		}
	}
}

void registerAccount() {
	char user[kMaxLine], passwd[kMaxLine], birthday[kMaxLine],
		 name[kMaxLine], gender[kMaxLine], intro[kMaxLine];
	bool retry = false;

	clear();
	printw("Register\n");

	inputUntilCorrect("  Username (at most 20 characters of alphabet, digit or '_', must begin with an alphabet):",
					  user, validUsername, "  Invalid username!\n");

	noecho();
	inputUntilCorrect("  Password (at least 8 characters, 32 at most, your input will be invisible):",
					  passwd, validPasswd, "  Try again\n");
	echo();

	inputUntilCorrect("  Your birthday? (Y-m-d):",
			birthday, validBirthday, "  Invalid input!\n");

	inputUntilCorrect("  Your full name please:",
			name, validName, "  Retry");

	inputUntilCorrect("  Gender(M/F):", 
			gender, validGender, "  A single character M or F please.");

	inputUntilCorrect("  A short introduction (no more than 70 characters):",
			intro, validIntro, "  Too long!");

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

	login();
}

void login() {
	bool retry = true;
	vector<RecordHandle> query_res;
	while (retry) {
		echo();
		printw("Username:");
		char user[kMaxLine],passwd[kMaxLine];
		getstr(user);
		printw("Password:");
		noecho();
		getstr(passwd);
		DBData user_d(DBType::STRING);
		user_d.str = user;
		query_res = db->query("userinfo","user",user_d);
		if (query_res.empty()) {
			printw("User not found!\n");
		} else {
			string correct_passwd = db->get(query_res[0],"passwd").str;
			if (strcmp(correct_passwd.c_str(),passwd) == 0)
				retry = false;
			else
				printw("Password not correct!\n");
		}
	}
	DBData uid_d = db->get(query_res[0],"id");
	uid = uid_d.int64;
	home();
}

/* Helper functions */

void inputUntilCorrect(const char *prompt, char *input, bool(*test)(char*),
		const char *failprompt) {
	bool retry = true;
	while (retry) {
		printw(prompt);
		getstr(input);
		if (test(input))
			retry = false;
		else
			printw(failprompt);
	}
}

bool validBirthday(char *birthday) {
	char buf[kMaxLine];
	struct tm raw = {0};
	// %F stands for %Y-%m-%d
	strptime(birthday, "%F", &raw);
	mktime(&raw);
	if (strftime(buf, sizeof(buf), "%F", &raw) == 0)
		return false;
	refresh();
	if (strcmp(birthday, buf) == 0)
		return true;
	return false;
}

bool validPasswd(char *passwd) {
	size_t len = strlen(passwd);
	if (len < 8 || len > 32)
		return false;

	char input[kMaxLine];
	printw("  Confirm your password:");
	getstr(input);
	if (strcmp(input, passwd) == 0)
		return true;
	printw("  Password did not match!\n");
	return false;
}

bool validUsername(char *user) {
	// A valid username is at most 20 characters, the first of which being
	// alphabet, others can be digit or '_'
	size_t len = strlen(user);
	if (len < 1 || len > 20)
		return false;
	if (!isalpha(user[0]))
		return false;
	for (size_t i = 1; i != len; ++i)
		if (!isalpha(user[i]) && !isdigit(user[i]) && user[i] != '_')
			return false;
	// check if the username already exists
	DBData query_dat;
	query_dat.type = DBType::STRING;
	query_dat.str = user;
	vector<RecordHandle> query_res = db->query("userinfo","user",query_dat);
	return query_res.empty();
}

bool validName(char *name) {
	if (strlen(name) > 28) // It's magic isn't it
		return false;
	return true;
}

bool validGender(char *raw) {
	if (strcmp(raw, "M") == 0 || strcmp(raw, "F") == 0)
		return true;
	return false;
}

bool validIntro(char *line) {
	if (strlen(line) > 74) // Magic!
		return false;
	return true;
}

bool validTweet(char *tweet) {
	if (strlen(tweet) > 140)
		return false;
	return true;
}
