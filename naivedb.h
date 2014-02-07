#ifndef NAIVEDB_H
#define NAIVEDB_H

#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include "kikutil.h"

/*
 * Disk storage
 * ----------------
 * The database file foo.xml stores database scheme
 * where table structure can be found
 * Each stable stores its data in tabname.dat and
 * indexes are stored in tabname_colname.idx file
 *
 * File schema can be found in filescheme.txt
 */

enum class DBType {
	INT32, INT64, STRING, BOOLEAN, ERROR
};

struct DBData {
	DBType type_;
	union {
		bool boolean;
		int int32;
		int64_t int64;
		std::string str;
	};
};

class NaiveDB {
	DISALLOW_COPY_AND_ASSIGN(NaiveDB);
private:
	// Class definitions

	struct Column {
		std::string name;
		bool indexed;
		bool unique;
		DBType type;
		int length;
	};
	struct Table {
		int data_length;
		std::vector<Column> scheme;
		std::unordered_map<std::string,int> colname_index;
	};
	// Data members

	std::unordered_map<std::string, Table> tables_;

	// Helper functions

	void loadMeta_(const std::string &dbname);
public:
	// Public methods

	void debug(); // run debug commands
	void insert(const std::string &tabname, std::vector<DBData> line);
	void del(const std::string &tabname, DBData primary_key);
	void modify(const std::string &tabname, const std::string &colname,
				DBData val);

	/*
	DBData get(const std::string &tabname, const std::string &key_col,
			   DBData key, const std::string &dest_col);
			   */
	std::vector<DBData> rangeGet(const std::string &tabname,
								 const std::string &key_col,
								 DBData first, DBData last,
								 const std::string &dest_col);
	// Constructor and destructor

	NaiveDB(const std::string &dbname);
	~NaiveDB();
};

#endif // NAIVEDB_H
