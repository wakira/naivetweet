#include <algorithm>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <clocale>
#include <cctype>
#include <ncurses.h>
#include "naivedb.h"
#include "tweetop.h"

#include <iostream>

using namespace std;

static const size_t kMaxLine = 80;

void welcome();
void login();
void registerAccount();
void home();
void newTweet();
void changeProfile();
void viewTweets();
void findPeople();
void viewPeople(RecordHandle handle);
void listFriends();
void tweetPageView(const vector<TweetLine> &alltweets);
void userPageView(const vector<RecordHandle> &allusers_handle);

/* Helper functions */
void retweet(const TweetLine &tweet);
void follow(int64_t id);
void unfollow(int64_t id);
void inputUntilCorrect(const char *prompt, char *input, bool(*test)(char*), const char *failprompt);
bool validUsername(char *user);
bool validPasswd(char *passwd);
bool validBirthday(char *birthday);
bool validName(char *name);
bool validGender(char *raw);
bool validIntro(char *line);
bool validTweet(char *tweet);

/* Global variables */
NaiveDB *db;
int64_t uid;

void debug() {
	db = new NaiveDB("benchmark.xml");
	for (long i = 0; i != 100000; ++i) {
		vector<DBData> line;
		DBData dbd(DBType::INT64);
		dbd.int64 = i;
		line.push_back(dbd);
		line.push_back(dbd);
		db->insert("bmtable",line);
	}
	for (long i = 0; i != 100000; ++i) {
		DBData dbd(DBType::INT64);
		dbd.int64 = i;
		RecordHandle handle = db->query("bmtable","idxtest",dbd)[0];
		printf("%d ",db->get(handle,"idxtest").int64);
	}
	printf("\n");
	delete db;
}

int main()
{
	setlocale(LC_ALL,"");
	initscr();
	cbreak();

	db = new NaiveDB("db.xml");

	welcome();

	endwin();
	delete db;
	return 0;
}

void follow(int64_t id) {
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

void unfollow(int64_t id) {
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

void changeProfile() {
	clear();
	DBData uid_d(DBType::INT64);
	uid_d.int64 = uid;
	RecordHandle handle = db->query("userinfo","id",uid_d).at(0);
	printw("What do you want to change?\n");
	printw("[1] Full name\n");
	printw("[2] Introduction\n");
	printw("[3] Password\n");
	printw("[x] to return home\n");
GOTO_TAG_changeProfile:
	noecho();
	char select = getch();
	echo();
	switch (select) {
	case '1': {
		char name[kMaxLine];
		inputUntilCorrect("New full name:",name,validName,"Invalid name!");
		DBData name_d(DBType::STRING);
		name_d.str = name;
		db->modify(handle,"name",name_d);
		printw("Profile updated!\n");
		break;
	}
	case '2': {
		char intro[kMaxLine];
		inputUntilCorrect("  A short introduction (no more than 70 characters):",
				intro, validIntro, "  Too long!");
		DBData intro_d(DBType::STRING);
		intro_d.str = intro;
		db->modify(handle,"introduction",intro_d);
		printw("Profile updated!\n");
		break;
	}
	case '3': {
		char passwd[kMaxLine], newpasswd[kMaxLine];
		printw("Original password:");
		noecho();
		getstr(passwd);
		string correct_passwd = db->get(handle,"passwd").str;
		if (strcmp(correct_passwd.c_str(),passwd) != 0) {
			printw("Password not correct!\n");
		} else {
			inputUntilCorrect("New password (at least 8 characters, 32 at most):", newpasswd,
							  validPasswd, "Retry!");
			DBData newpasswd_d(DBType::STRING);
			newpasswd_d.str = newpasswd;
			db->modify(handle,"passwd",newpasswd_d);
			printw("Profile updated!\n");
		}
		break;
	}
	case 'x': case 'X': {
		break;
	}
	default: {
		goto GOTO_TAG_changeProfile;
	}
	}
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
}

void viewTweets() {
	clear();
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
	clear();
}

void retweet(const TweetLine &tweet) {
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

void tweetPageView(const vector<TweetLine> &alltweets) {
	static const int kTweetPerPage = 15;
	int page = 1;
	int max_page = (alltweets.size() - 1)/kTweetPerPage + 1;
	if (alltweets.empty())
		max_page = 1;
	bool noexit = true;
	while (noexit) {
		clear();
		printw("Page : %d/%d\n",page,max_page);
		// print tweet
		for (int i = (page-1)*kTweetPerPage;
			 i < std::min(alltweets.size(),(size_t)page*kTweetPerPage);
			 ++i) {
			TweetLine tweet = alltweets[i];
			char timestr[kMaxLine];
			time_t timet = tweet.time;
			strftime(timestr,sizeof(timestr),"%F %T",localtime(&timet));
			if (tweet.author == tweet.publisher) {
				// query publisher name
				DBData uid_d(DBType::INT64);
				uid_d.int64 = tweet.publisher;
				RecordHandle handle = db->query("userinfo","id",uid_d).at(0);
				string usrstr = db->get(handle,"user").str;
				printw("[%d] %s: %s (%s)\n",i,usrstr.c_str(),tweet.content.c_str(),timestr);
			} else {
				// query publisher name and author name
				DBData uid_d(DBType::INT64), author_uid_d(DBType::INT64);
				uid_d.int64 = tweet.publisher;
				author_uid_d.int64 = tweet.author;
				RecordHandle handle = db->query("userinfo","id",uid_d).at(0);
				RecordHandle author_handle = db->query("userinfo","id",author_uid_d).at(0);
				string usrstr = db->get(handle,"user").str;
				string autstr = db->get(author_handle,"user").str;
				printw("[%d] %s:RT @%s: %s (%s)\n",i,usrstr.c_str(),autstr.c_str(),tweet.content.c_str(),timestr);
			}
		}
		// process key press
		printw("\n[j] for next page, [k] for previous page\n");
		printw("[r] to retweet, [x] to return home\n");
		char keypress;
		noecho();
		while (keypress = getch()) {
			if (keypress == 'j' || keypress == 'J') {
				if (page < max_page) {
					++page;
					break;
				}
			}
			if (keypress == 'k' || keypress == 'K') {
				if (page > 1) {
					--page;
					break;
				}
			}
			if (keypress == 'r' || keypress == 'R') {
				printw("Which one to retweet? (input number in []):");
				echo();
				char input[kMaxLine];
				getstr(input);
				int choice = atoi(input);
				if (choice >= 0 && choice < alltweets.size()) {
					retweet(alltweets[choice]);
					noexit = false;
					break;
				} else {
					printw("Invalid choice!\n");
				}
				noecho();
			}
			if (keypress == 'x' || keypress == 'X') {
				noexit = false;
				break;
			}
		}
	}
}

void viewPeople(RecordHandle handle) {
	clear();
	char name[kMaxLine],user[kMaxLine],gender[kMaxLine],birthday[kMaxLine],intro[kMaxLine];
	strcpy(user,db->get(handle,"user").str.c_str());
	strcpy(name,db->get(handle,"name").str.c_str());
	strcpy(birthday,db->get(handle,"birthday").str.c_str());
	strcpy(intro,db->get(handle,"introduction").str.c_str());
	bool male_b = db->get(handle,"male").boolean;
	if (male_b)
		sprintf(gender,"Male");
	else
		sprintf(gender,"Female");
	int64_t id = db->get(handle,"id").int64;

	printw("Username: %s\n",user);
	printw("Full name: %s\n",name);
	printw("Gender: %s\n",gender);
	printw("Birthday: %s\n",birthday);
	printw("Introduction: %s\n",intro);

	noecho();

	if (id != uid) {
		bool following = false;
		DBData uid_d(DBType::INT64);
		uid_d.int64 = uid;
		vector<RecordHandle> query_res = db->query("afob","a",uid_d);
		for (RecordHandle handle : query_res) {
			if (id == db->get(handle,"b").int64 &&
					db->get(handle,"deleted").boolean == false) {
				following = true;
				break;
			}
		}

		if (following) {
			printw("[u] to unfo [x] to return\n");
			char keypress;
			while (keypress = getch()) {
				if (keypress == 'u' || keypress == 'U') {
					unfollow(id);
					break;
				}
				if (keypress == 'x' || keypress == 'X')
					break;
			}
		} else {
			printw("[f] to follow [x] to return\n");
			char keypress;
			while (keypress = getch()) {
				if (keypress == 'f' || keypress == 'F') {
					follow(id);
					break;
				}
				if (keypress == 'x' || keypress == 'X')
					break;
			}
		}
	} else {
		printw("How could you forget your own profile? Press any key to return\n");
		getch();
	}
	clear();
}

void userPageView(const vector<RecordHandle> &allusers_handle) {
	static const int kUserPerPage = 15;
	int page = 1;
	int max_page = (allusers_handle.size() - 1)/kUserPerPage + 1;
	bool noexit = true;
	while (noexit) {
		clear();
		printw("Page : %d/%d\n",page,max_page);
		// print user
		for (int i = (page-1)*kUserPerPage;
			 i != std::min(allusers_handle.size(),(size_t)page*kUserPerPage);
			 ++i) {
			RecordHandle handle = allusers_handle[i];
			printw("[%d] %s\n",i,db->get(handle,"user").str.c_str());
		}
		// process key press
		printw("\n[j] for next page, [k] for previous page\n");
		printw("[d] for detail, [x] to return\n");
		char keypress;
		noecho();
		while (keypress = getch()) {
			if (keypress == 'j' || keypress == 'J') {
				if (page < max_page) {
					++page;
					break;
				}
			}
			if (keypress == 'k' || keypress == 'K') {
				if (page > 1) {
					--page;
					break;
				}
			}
			if (keypress == 'd' || keypress == 'D') {
				printw("Which user to see? (input number in []):");
				echo();
				char input[kMaxLine];
				getstr(input);
				int choice = atoi(input);
				if (choice >= 0 && choice < allusers_handle.size()) {
					viewPeople(allusers_handle[choice]);
					break;
				} else {
					printw("Invalid choice!\n");
				}
				noecho();
			}
			if (keypress == 'x' || keypress == 'X') {
				noexit = false;
				break;
			}
		}
	}
}

void findPeople() {
	clear();
	printw("[1] by username\n");
	printw("[2] by birthday and gender\n");
	printw("[3] by full name\n");
	noecho();
	char keypress;
	keypress = getch();
	switch (keypress) {
	case '1': {
		printw("Input user name:");
		char user[kMaxLine];
		echo();
		getstr(user);
		DBData query_d(DBType::STRING);
		query_d.str = user;
		vector<RecordHandle> query_res = db->query("userinfo","user",query_d);
		if (query_res.empty()) {
			printw("Not found!\n");
			return;
		}
		viewPeople(query_res[0]);
		break;
	}
	case '2': {
		char birthday_f[kMaxLine], birthday_l[kMaxLine];
		char gender[kMaxLine];
		echo();
		inputUntilCorrect("Birthday from:",birthday_f,validBirthday,"Invalid format!");
		inputUntilCorrect("to:",birthday_l,validBirthday,"Invalid format!");
		inputUntilCorrect("Gender (M/F):",gender,validGender,"Invalid input!");
		DBData birthday_f_d(DBType::STRING), birthday_l_d(DBType::STRING);
		birthday_f_d.str = birthday_f;
		birthday_l_d.str = birthday_l;
		bool male_b;
		if (strcmp(gender,"M") == 0)
			male_b = true;
		else
			male_b = false;
		vector<RecordHandle> query_res = db->rangeQuery("userinfo","birthday",
														birthday_f_d,birthday_l_d);
		vector<RecordHandle> allusers_handle;
		for (RecordHandle handle : query_res) {
			if (db->get(handle,"deleted").boolean == false &&
					db->get(handle,"male").boolean == male_b) {
				// user that meet the requirement
				allusers_handle.push_back(handle);
			}
		}
		userPageView(allusers_handle);
		break;
	}
	case '3': {
		printw("Input full name:");
		char name[kMaxLine];
		echo();
		getstr(name);
		DBData query_d(DBType::STRING);
		query_d.str = name;
		vector<RecordHandle> query_res = db->query("userinfo","name",query_d);
		if (query_res.empty()) {
			printw("Not found!\n");
			return;
		}
		viewPeople(query_res[0]);
		break;
	}
	}
}

void listFriends() {
	clear();
	DBData uid_d(DBType::INT64);
	uid_d.int64 = uid;
	vector<RecordHandle> query_res = db->query("afob","a",uid_d);
	vector<int64_t> following_uid;
	vector<string> following_user;
	vector<RecordHandle> user_handles;
	for (RecordHandle x : query_res) {
		bool deleted = db->get(x,"deleted").boolean;
		if (!deleted) {
			DBData x_uid_d = db->get(x,"b");
			int64_t x_uid = x_uid_d.int64;
			following_uid.push_back(x_uid);
			RecordHandle userinfo_query = db->query("userinfo","id",x_uid_d).at(0);
			user_handles.push_back(userinfo_query);
			following_user.push_back(db->get(userinfo_query,"user").str);
		}
	}
	printw("People you are following:\n");
	for (size_t i = 0; i != following_uid.size(); ++i)
		printw("[%d] %s\n",i,following_user[i].c_str());
	printw("[d] for detail, [x] to return\n");
	char keypress;
	noecho();
	while (keypress = getch()) {
		if (keypress == 'd' || keypress == 'D') {
			printw("Which user to see? (input number in []):");
			echo();
			char input[kMaxLine];
			getstr(input);
			int choice = atoi(input);
			if (choice >= 0 && choice < following_uid.size()) {
				viewPeople(user_handles[choice]);
				break;
			} else {
				printw("Invalid choice!\n");
			}
			noecho();
		}
		if (keypress == 'x' || keypress == 'X') {
			break;
		}
	}
}

void home() {
GOTO_TAG_home:
	printw("[t] to tweet\n");
	printw("[v] to view tweets\n");
	printw("[f] to find people\n");
	printw("[l] to list friends\n");
	printw("[r] to change profile\n");
	printw("[x] to exit\n");
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
		findPeople();
		break;
	case 'l': case 'L':
		listFriends();
		break;
	case 'r': case 'R':
		changeProfile();
		break;
	case 'x': case 'X':
		return;
	default:
		clear();
	}
	goto GOTO_TAG_home;
}

void welcome() {
	printw("Welcome to naivetweet, press 'l' to login, 'r' to register, 'x' to quit\n");
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
		if (c == 'x' || c == 'X') {
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

	TweetOp::registerAccount(db,user,passwd,birthday,name,gender,intro);
	login();
}

void login() {
	bool retry = true;
	vector<RecordHandle> query_res;
	int64_t login_res;
	while (retry) {
		echo();
		printw("Username:");
		char user[kMaxLine],passwd[kMaxLine];
		getstr(user);
		printw("Password:");
		noecho();
		getstr(passwd);

		login_res = TweetOp::login(db,user,passwd);
		if (login_res == -1)
			printw("User not found!\n");
		else if (login_res == 0)
			printw("Incorrect password!\n");
		else
			break; // success
	}
	uid = login_res;
	clear();
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
	return TweetOp::userExist(db,user);
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
