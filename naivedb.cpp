#include <cassert>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include "naivedb.h"
#include "diskfile.h"

using namespace std;
using namespace boost::property_tree;

void NaiveDB::debug() {

}

inline DBType getTypeFromStr(const string &str) {
	if (str == "int32")
		return DBType::INT32;
	if (str == "int64")
		return DBType::INT64;
	if (str == "string")
		return DBType::STRING;
	if (str == "boolean")
		return DBType::BOOLEAN;
	return DBType::ERROR;
}

// read metadata
void NaiveDB::loadMeta_(const string &dbname) {
	ptree pt;
	read_xml(dbname, pt);
	pt = pt.get_child("database.tables");

	// For every table
	for (auto tab_iter : pt) {
		ptree tab_pt = tab_iter.second;
		// Get table name
		string tabname = tab_pt.get<string>("name");
		// Initialize table
		tables_[tabname].data_length = 0;

		// Load each column info
		ptree cols_pt = tab_pt.get_child("columns");
		for (auto col_iter : cols_pt) {
			ptree col = col_iter.second;
			Column newcol;
			// <name>
			newcol.name = col.get<string>("name");
			// <type>
			newcol.type = getTypeFromStr(col.get<string>("type"));
			// <length>
			switch (newcol.type) {
			case DBType::INT32:
				newcol.length = 4;
				break;
			case DBType::INT64:
				newcol.length = 8;
				break;
			case DBType::BOOLEAN:
				newcol.length = 1;
				break;
			case DBType::STRING:
				newcol.length = col.get<int>("length");
				break;
			default:
				assert(0);
			}
			// <index>
			if (col.get<string>("index") == "true")
				newcol.indexed = true;
			else
				newcol.indexed = false;
			// <unique>
			if (col.get<string>("unique") == "true")
				newcol.unique = true;
			else
				newcol.unique = false;

			// add the column to table
			tables_[tabname].colname_index[newcol.name] =
					tables_[tabname].scheme.size();
			tables_[tabname].scheme.push_back(newcol);
			// add column size to the table size counter
			tables_[tabname].data_length += newcol.length;
		}
	}
}

NaiveDB::NaiveDB(const string &dbname) {
	loadMeta_(dbname);
}

void NaiveDB::insert(const string &tabname, std::vector<DBData> line) {
	Table target_tab = tables_.at(tabname);
	fstream tabfile(tabname + ".dat", ios::binary);
	int64_t new_pid = DatFile::increasePrimaryId(tabfile);
	// find a free chunk and modify meta information
	DatFile::consumeFreeSpace(tabfile);
	// write primary id
	binary_write(tabfile, new_pid);
	// write each column
	for (size_t i = 0; i != target_tab.scheme.size(); ++i) {
		switch (target_tab.scheme[i].type) {
		case DBType::BOOLEAN:
			binary_write(tabfile, line[i].boolean);
			break;
		case DBType::INT32:
			binary_write(tabfile, line[i].int32);
			break;
		case DBType::INT64:
			binary_write(tabfile, line[i].int64);
			break;
		case DBType::STRING: {
			size_t j = 0;
			// j <= str.length() means that '\0' is also writen
			for (; j <= line[i].str.length(); ++j)
				binary_write(tabfile, line[i].str[j]);
			// space holder when provided string is shorter
			for (; j != target_tab.scheme[i].length; ++j)
				binary_write(tabfile, '\0');
			break;
		}
		default:
			// DBType::ERROR falls in
			assert(0);
		}
		// TODO create index for this column
		if (target_tab.scheme[i].indexed) {

		}
	}
}

void NaiveDB::del(const string &tabname, DBData primary_key) {
	Table target_tab = tables_.at(tabname);
	fstream tabfile(tabname + ".dat", ios::binary);
	// TODO locate record by index
}
