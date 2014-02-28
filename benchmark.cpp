#include <iostream>
#include <ctime>
#include "naivedb.h"

using namespace std;

int main() {
	NaiveDB db("benchmark.xml");
	cout << "Insertion:" << endl;
	clock_t before_insert = clock();
	for (long ii = 0; ii != 1000000; ii += 50000) {
		long i;
		for (i = ii; i != ii + 50000; ++i) {
			vector<DBData> line;
			DBData dbd(DBType::INT64);
			dbd.int64 = i;
			line.push_back(dbd);
			db.insert("bmtable",line);
		}
		clock_t after_insert = clock();
		cout << "  " << i << " completed after " << 
			((double)(after_insert - before_insert)/CLOCKS_PER_SEC*1000) << "ms" << endl;
	}

	cout << "Query:" << endl;
	before_insert = clock();
	for (long ii = 1; ii <= 1000000; ii += 50000) {
		long i;
		for (i = ii; i != ii + 50000; ++i) {
			DBData dbd(DBType::INT64);
			dbd.int64 = i;
			db.query("bmtable","id",dbd);
		}
		clock_t after_insert = clock();
		cout << "  " << i - 1 << " completed after " << 
			((double)(after_insert - before_insert)/CLOCKS_PER_SEC*1000) << "ms" << endl;
	}

	before_insert = clock();
	DBData dbd_f(DBType::INT64);
	dbd_f.int64 = 0;
	DBData dbd_l(DBType::INT64);
	dbd_l.int64 = 1000000;
	db.rangeQuery("bmtable","id",dbd_f,dbd_l);
	clock_t after_insert = clock();
	cout << "Range query completed after " << 
		((double)(after_insert - before_insert)/CLOCKS_PER_SEC*1000) << "ms" << endl;

	cout << "Verifying" << endl;
	bool success = true;
	for (long i = 1; i <= 1000000; ++i) {
		DBData dbd(DBType::INT64);
		dbd.int64 = i;
		RecordHandle handle = db.query("bmtable","id",dbd)[0];
		int64_t ret = db.get(handle,"id").int64;
		if (ret != i) {
			cout << "  Fail" << endl;
			success = false;
			break;
		}
	}
	if (success)
		cout << "  OK!" << endl;

	return 0;
}
