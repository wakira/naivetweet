#ifndef NAIVEDB_H
#define NAIVEDB_H

#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include "kikutil.h"
#include "bptree.hpp"

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
	DBType type;
	char boolean;
	int int32;
	int64_t int64;
	std::string str;

	DBData() = default;
	DBData(DBType fromtype) : type(fromtype) {}

	bool operator==(const DBData &rval);
	bool operator!=(const DBData &rval);
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
		size_t length;
	};
	struct Table {
		size_t data_length;
		std::vector<Column> schema;
		std::unordered_map<std::string,int> colname_index;
		std::unordered_map<std::string, void*> bptree;
	};
	// Data members

	std::unordered_map<std::string, Table> tables_;

	// Helper functions

	void* newBPTree_(const std::string &tabname,const Column &col);
	void insertInBPTree_(void* bptree,const Column &col,DBData key,FilePos value);
	std::vector<FilePos> findInBPTree_(void* bptree,const Column &col,DBData key);
	void loadMeta_(const std::string &dbname);
	void loadIndex_();
	DBData getDBData_(std::fstream &stream,DBType type);
	DBData getDBDataAtPos_(std::fstream &stream,DBType type,FilePos pos);
	bool compareDBDataAtPos_(std::fstream &stream,FilePos pos,DBData comp);
	int calculateColumnOffset_(const std::string &tabname,const std::string &colname);
public:
	// Public methods

	void debug(); // run debug commands
	void insert(const std::string &tabname, std::vector<DBData> line);
	void del(const std::string &tabname, DBData primary_key);
	void modify(const std::string &tabname, const std::string &colname,
				DBData val);
	std::vector<DBData> get(const std::string &tabname, const std::string &key_col,
			   DBData key, const std::string &dest_col);
	std::vector<DBData> rangeGet(const std::string &tabname,
								 const std::string &key_col,
								 DBData first, DBData last,
								 const std::string &dest_col);
	// Constructor and destructor

	NaiveDB(const std::string &dbname);
	~NaiveDB();
};

#endif // NAIVEDB_H
