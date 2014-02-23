#include <cassert>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include "naivedb.h"
#include "diskfile.h"
#include "spstring.hpp"

using namespace std;
using namespace boost::property_tree;

bool DBData::operator ==(const DBData &rval) {
	if (type != rval.type)
		return false;
	switch (type) {
	case DBType::BOOLEAN:
		return boolean == rval.boolean;
	case DBType::INT32:
		return int32 == rval.int32;
	case DBType::INT64:
		return int64 == rval.int64;
	case DBType::STRING:
		return str == rval.str;
	default:
		assert(0);
	}
}

bool DBData::operator !=(const DBData &rval) {
	return !(*this == rval);
}

DBData NaiveDB::getDBData_(fstream &stream,DBType type) {
	DBData retval;
	retval.type = type;
	switch (type) {
	case DBType::BOOLEAN: {
		char byte;
		binary_read(stream,byte);
		retval.boolean = byte;
		return retval;
	}
	case DBType::INT32: {
		int32_t int32;
		binary_read(stream,int32);
		retval.int32 = int32;
		return retval;
	}
	case DBType::INT64: {
		int64_t int64;
		binary_read(stream,int64);
		retval.int64 = int64;
		return retval;
	}
	case DBType::STRING: {
		// read until '\0'
		char byte;
		while (true) {
			binary_read(stream,byte);
			if (byte == '\0')
				break;
			retval.str.push_back(byte);
		}
		return retval;
	}
	default: {
		assert(0);
	}
	}
}

DBData NaiveDB::getDBDataAtPos_(fstream &stream, DBType type, FilePos pos) {
	stream.seekg(pos);
	return getDBData_(stream,type);
}

bool NaiveDB::compareDBDataAtPos_(fstream &stream, FilePos pos, DBData comp) {
	DBData gotval = getDBDataAtPos_(stream,comp.type,pos);
	return gotval == comp;
}

std::vector<FilePos> NaiveDB::findInBPTree_(void *bptree, const Column &col, DBData key) {
	switch (col.type) {
	case DBType::INT32: {
		BPTree<int32_t,FilePos> *tree = static_cast<BPTree<int32_t,FilePos>*>(bptree);
		return tree->find(key.int32);
	}
	case DBType::INT64: {
		BPTree<int64_t,FilePos> *tree = static_cast<BPTree<int64_t,FilePos>*>(bptree);
		return tree->find(key.int64);
	}
	case DBType::STRING: {
		BPTree<string,FilePos> *tree = static_cast<BPTree<string,FilePos>*>(bptree);
		return tree->find(key.str);
	}
	default: {
		assert(0);
	}
	}
}

void NaiveDB::insertInBPTree_(void* bptree,const Column &col,DBData key, FilePos value) {

	switch (col.type) {
	case DBType::INT32: {
		BPTree<int32_t,FilePos> *tree = static_cast<BPTree<int32_t,FilePos>*>(bptree);
		tree->insert(key.int32,value);
		return;
	}
	case DBType::INT64: {
		BPTree<int64_t,FilePos> *tree = static_cast<BPTree<int64_t,FilePos>*>(bptree);
		tree->insert(key.int32,value);
		return;
	}
	case DBType::STRING: {
		BPTree<string,FilePos> *tree = static_cast<BPTree<string,FilePos>*>(bptree);
		tree->insert(key.str,value);
		return;
	}
	default: {
		assert(0);
	}
	}
}

void* NaiveDB::newBPTree_(const string &tabname, const Column &col) {
	void* addr;
	string filename = tabname + "_" + col.name + ".idx";
	switch (col.type) {
	case DBType::INT32:
		addr = new BPTree<int32_t,FilePos>(filename);
		return addr;
	case DBType::INT64:
		addr = new BPTree<int64_t,FilePos>(filename);
		return addr;
	case DBType::STRING:
		addr = new BPTree<string,FilePos>(filename,col.length);
		return addr;
	default:
		assert(0);
	}

}

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

		// add pid info in schema
		Column idcol;
		idcol.name = "id";
		idcol.length = 8;
		idcol.indexed = true;
		idcol.type = DBType::INT64;
		idcol.unique = true;
		idcol.offset = 0;
		tables_[tabname].colname_index[idcol.name] =
				tables_[tabname].schema.size();
		tables_[tabname].schema.push_back(idcol);
		tables_[tabname].data_length += idcol.length;
		// Load each other column info
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
			// set offset
			newcol.offset = tables_[tabname].data_length;

			// add the column to table
			tables_[tabname].colname_index[newcol.name] =
					tables_[tabname].schema.size();
			tables_[tabname].schema.push_back(newcol);
			// add column size to the table size counter
			tables_[tabname].data_length += newcol.length;
		}
	}
}

void NaiveDB::loadIndex_() {
	for (auto iter : tables_) {
		string tabname = iter.first;
		for (Column col : iter.second.schema)
			if (col.indexed)
				iter.second.bptree[col.name] = newBPTree_(tabname,col);
	}
}

NaiveDB::NaiveDB(const string &dbname) {
	loadMeta_(dbname);
	checkDatFile_();
	loadIndex_();
}

void NaiveDB::checkDatFile_() {
	for (auto pair : tables_) {
		string filename = pair.first + ".dat";
		Table tab = pair.second;
		if (!fileExists(filename.c_str())) {
			// create an empty dat file
			fstream datfile(filename.c_str(), ios::out | ios::binary);
			datfile.seekp(DatFile::kRecordStartPos - 2);
			char byte = 0;
			datfile.put(byte);
			datfile.close();
		}
	}
}

void NaiveDB::insert(const string &tabname, std::vector<DBData> line) {
	Table target_tab = tables_.at(tabname);
	fstream tabfile(tabname + ".dat", ios::in | ios::out | ios::binary);
	int64_t new_pid = DatFile::increasePrimaryId(tabfile);
	// find a free chunk and modify meta information
	FilePos record_pos = DatFile::consumeFreeSpace(tabfile);
	// write primary id
	binary_write(tabfile, new_pid);
	// write each column
	// i starts from 1 because pid has already been written
	for (size_t i = 1; i != target_tab.schema.size(); ++i) {
		switch (target_tab.schema[i].type) {
		case DBType::BOOLEAN:
			binary_write(tabfile, line[i-1].boolean);
			break;
		case DBType::INT32:
			binary_write(tabfile, line[i-1].int32);
			break;
		case DBType::INT64:
			binary_write(tabfile, line[i-1].int64);
			break;
		case DBType::STRING: {
			size_t j = 0;
			// j <= str.length() means that '\0' is also writen
			for (; j != line[i-1].str.length(); ++j)
				binary_write(tabfile,line[i-1].str[j]);
			char byte = '\0';
			binary_write(tabfile,byte);
			++j;
			// space holder when provided string is shorter
			for (; j != target_tab.schema[i].length; ++j)
				binary_write(tabfile,byte);
			break;
		}
		default:
			// DBType::ERROR falls in
			assert(0);
		}
		// create index for this column
		if (target_tab.schema[i].indexed) {
			string colname = target_tab.schema[i].name;
			insertInBPTree_(target_tab.bptree[colname],
							target_tab.schema[i],line[i-1],record_pos);
		}
	}
}

void NaiveDB::del(const string &tabname, DBData primary_key) {
	Table target_tab = tables_.at(tabname);
	fstream tabfile(tabname + ".dat", ios::in | ios::out | ios::binary);
	// TODO locate record by index
}

DBData NaiveDB::get(RecordHandle handle, const string &dest_col) {
	Table target_tab = tables_.at(handle.tabname);
	int dest_col_index = target_tab.colname_index.at(dest_col);
	Column destcol = target_tab.schema.at(dest_col_index);
	fstream tabfile(handle.tabname + ".dat", ios::in | ios::binary);
	return getDBDataAtPos_(tabfile,destcol.type,handle.filepos + destcol.offset);
}

std::vector<RecordHandle> NaiveDB::query(const string &tabname,
					const string &key_col, DBData key) {
	std::vector<RecordHandle> retval;
	Table target_tab = tables_.at(tabname);
	int col_index = target_tab.colname_index.at(key_col);
	Column col = target_tab.schema.at(col_index);
	fstream tabfile(tabname + ".dat", ios::in | ios::out | ios::binary);
	if (col.indexed) {
		// indexed way
		vector<FilePos> retpos = findInBPTree_(target_tab.bptree[key_col],col,key);
		for (FilePos x : retpos)
			retval.push_back(RecordHandle(tabname,x));
		return retval;
	} else {
		// full scan
		FilePos current_record = DatFile::kRecordStartPos;
		tabfile.seekg(0,tabfile.end);
		FilePos eofpos = tabfile.tellg();
		if (col.unique) {
			for (; current_record < eofpos;
					 current_record += target_tab.data_length + 1) {
				if (DatFile::isRecordDeleted(tabfile,current_record))
					continue;
				if (compareDBDataAtPos_(tabfile,current_record + col.offset,key)) {
					RecordHandle record(tabname,current_record);
					retval.push_back(record);
					return retval;
				}
			}

		} else {
			for (; current_record < eofpos;
					 current_record += target_tab.data_length + 1) {
				if (DatFile::isRecordDeleted(tabfile,current_record))
					continue;
				if (compareDBDataAtPos_(tabfile,current_record + col.offset,key)) {
					RecordHandle record(tabname,current_record);
					retval.push_back(record);
				}
			}
			return retval;
		}
	}
}

std::vector<RecordHandle> NaiveDB::rangeQuery(const string &tabname,
				  const string &key_col, DBData first, DBData last) {
	std::vector<DBData> retval;
	Table target_tab = tables_.at(tabname);
	int col_index = target_tab.colname_index.at(key_col);
	Column col = target_tab.schema.at(col_index);
	// TODO
}
