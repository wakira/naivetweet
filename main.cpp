#include <cstring>
#include <ctime>
#include <clocale>
#include <cctype>
#include <ncurses.h>
#include "naivedb.h"

using namespace std;

const size_t kMaxLine = 80;

void welcome();
void login();
void registerAccount();

/* Helper functions */
void inputUntilCorrect(const char *prompt, char *input, bool(*test)(char*), const char *failprompt);
bool validUsername(char *user);
bool validPasswd(char *passwd);
bool validBirthday(char *birthday);
bool validName(char *name);
bool validGender(char *raw);
bool validIntro(char *line);

/* Global variables */
NaiveDB *db;

void debug() {
	BPTree<long,long> bp("test.idx");
	for (long i = 100; i >= 1; --i)
		bp.insert(i,i);
	for (long i = 1; i <= 100; ++i)
		printf("%d ",bp.find(i).at(0));
	printf("\n");
}

int main()
{
	debug();
	return 0;
	// DEBUG IS OVER
	setlocale(LC_ALL,"");
	initscr();
	cbreak();

	db = new NaiveDB("db.xml");

	welcome();

	endwin();
	return 0;
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

	// TODO
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
	vector<DBData> query_result = db->get("userinfo","username",query_dat,"id");
	return query_result.empty();
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
