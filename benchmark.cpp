#include <iostream>
#include <ctime>
#include "naivedb.h"

using namespace std;

int main() {
	NaiveDB db("benchmark.xml");
	clock_t before_insert = clock();
	for (long i = 0; i != 1000000; ++i) {
		vector<DBData> line;
		DBData dbd(DBType::INT64);
		dbd.int64 = i;
		line.push_back(dbd);
		db.insert("bmtable",line);
	}
	clock_t after_insert = clock();
	cout << "Insertion completed after " << 
		((after_insert - before_insert)/CLOCKS_PER_SEC) << "s" << endl;

	before_insert = clock();
	for (long i = 1; i <= 1000000; ++i) {
		DBData dbd(DBType::INT64);
		dbd.int64 = i;
		db.query("bmtable","id",dbd);
	}
	after_insert = clock();
	cout << "Query completed after " << 
		((after_insert - before_insert)/CLOCKS_PER_SEC) << "s" << endl;

	before_insert = clock();
	DBData dbd_f(DBType::INT64);
	dbd_f.int64 = 0;
	DBData dbd_l(DBType::INT64);
	dbd_l.int64 = 1000000;
	db.rangeQuery("bmtable","id",dbd_f,dbd_l);
	after_insert = clock();
	cout << "Range query completed after " << 
		((after_insert - before_insert)/CLOCKS_PER_SEC) << "s" << endl;
	return 0;
}
