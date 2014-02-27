#include <cassert>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include "naivedb.h"
#include "diskfile.h"

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

std::vector<FilePos> NaiveDB::rangeFindInBPTree_(void* bptree,const Column &col,DBData first,DBData last) {
	switch (col.type) {
	case DBType::INT32: {
		BPTree<int32_t,FilePos> *tree = static_cast<BPTree<int32_t,FilePos>*>(bptree);
		return tree->rangeFind(first.int32,last.int32);
	}
	case DBType::INT64: {
		BPTree<int64_t,FilePos> *tree = static_cast<BPTree<int64_t,FilePos>*>(bptree);
		return tree->rangeFind(first.int64,last.int64);
	}
	case DBType::STRING: {
		BPTree<string,FilePos> *tree = static_cast<BPTree<string,FilePos>*>(bptree);
		return tree->rangeFind(first.str,last.str);
	}
	default: {
		assert(0);
	}
	}
}

void NaiveDB::deleteBPTree_(void *bptree, const Column &col) {
	switch (col.type) {
	case DBType::INT32: {
		BPTree<int32_t,FilePos> *tree = static_cast<BPTree<int32_t,FilePos>*>(bptree);
		delete tree;
		break;
	}
	case DBType::INT64: {
		BPTree<int64_t,FilePos> *tree = static_cast<BPTree<int64_t,FilePos>*>(bptree);
		delete tree;
		break;
	}
	case DBType::STRING: {
		BPTree<string,FilePos> *tree = static_cast<BPTree<string,FilePos>*>(bptree);
		delete tree;
		break;
	}
	default: {
		assert(0);
	}
	}
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
		tree->insert(key.int64,value);
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
			if (col.get<string>("index") == "yes")
				newcol.indexed = true;
			else
				newcol.indexed = false;
			// <unique>
			if (col.get<string>("unique") == "yes")
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
	for (auto &iter : tables_) {
		string tabname = iter.first;
		for (Column &col : iter.second.schema)
			if (col.indexed)
				iter.second.bptree[col.name] = newBPTree_(tabname,col);
	}
}

NaiveDB::NaiveDB(const string &dbname) {
	loadMeta_(dbname);
	prepareDatFile_();
	loadIndex_();
}

void NaiveDB::prepareDatFile_() {
	for (auto &pair : tables_) {
		string filename = pair.first + ".dat";
		Table &tab = pair.second;
		if (!fileExists(filename.c_str())) {
			// create an empty dat file
			tab.fileptr = new fstream(filename, ios::out | ios::binary);
			char byte = 0;
			writeToPos(*tab.fileptr,DatFile::kRecordStartPos - 2,byte);
			tab.fileptr->close();
			tab.fileptr->open(filename, ios::in | ios::out | ios::binary);
		} else
			tab.fileptr = new fstream(filename, ios::in | ios::out | ios::binary);
	}
}

void NaiveDB::insert(const string &tabname, std::vector<DBData> line) {
	Table &target_tab = tables_.at(tabname);
	int64_t new_pid = DatFile::increasePrimaryId(*target_tab.fileptr);
	// find a free chunk and modify meta information
	FilePos record_pos = DatFile::consumeFreeSpace(*target_tab.fileptr);
	// write primary id
	binary_write(*target_tab.fileptr, new_pid);
	// create index for id
	DBData id_d(DBType::INT64);
	id_d.int64 = new_pid;
	insertInBPTree_(target_tab.bptree["id"],
			target_tab.schema[0],id_d,record_pos);
	// write each column
	// i starts from 1 because pid has already been written
	for (size_t i = 1; i != target_tab.schema.size(); ++i) {
		switch (target_tab.schema[i].type) {
		case DBType::BOOLEAN:
			binary_write(*target_tab.fileptr, line[i-1].boolean);
			break;
		case DBType::INT32:
			binary_write(*target_tab.fileptr, line[i-1].int32);
			break;
		case DBType::INT64:
			binary_write(*target_tab.fileptr, line[i-1].int64);
			break;
		case DBType::STRING:
			binary_write_s(*target_tab.fileptr,line[i-1].str,target_tab.schema[i].length);
			break;
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

DBData NaiveDB::get(RecordHandle handle, const string &dest_col) {
	Table &target_tab = tables_.at(handle.tabname);
	int dest_col_index = target_tab.colname_index.at(dest_col);
	Column destcol = target_tab.schema.at(dest_col_index);
	return getDBDataAtPos_(*target_tab.fileptr,destcol.type,handle.filepos + destcol.offset);
}

std::vector<RecordHandle> NaiveDB::query(const string &tabname,
					const string &key_col, DBData key) {
	std::vector<RecordHandle> retval;
	Table &target_tab = tables_.at(tabname);
	int col_index = target_tab.colname_index.at(key_col);
	Column col = target_tab.schema.at(col_index);
	if (col.indexed) {
		// indexed way
		vector<FilePos> retpos = findInBPTree_(target_tab.bptree[key_col],col,key);
		for (FilePos &x : retpos)
			retval.push_back(RecordHandle(tabname,x));
		return retval;
	} else {
		// full scan
		FilePos current_record = DatFile::kRecordStartPos;
		target_tab.fileptr->seekg(0,target_tab.fileptr->end);
		FilePos eofpos = target_tab.fileptr->tellg();
		if (col.unique) {
			for (; current_record < eofpos;
					 current_record += target_tab.data_length + 1) {
				if (DatFile::isRecordDeleted(*target_tab.fileptr,current_record))
					continue;
				if (compareDBDataAtPos_(*target_tab.fileptr,current_record + col.offset,key)) {
					RecordHandle record(tabname,current_record);
					retval.push_back(record);
					return retval;
				}
			}
			return retval;
		} else {
			for (; current_record < eofpos;
					 current_record += target_tab.data_length + 1) {
				if (DatFile::isRecordDeleted(*target_tab.fileptr,current_record))
					continue;
				if (compareDBDataAtPos_(*target_tab.fileptr,current_record + col.offset,key)) {
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
	std::vector<RecordHandle> retval;
	Table &target_tab = tables_.at(tabname);
	int col_index = target_tab.colname_index.at(key_col);
	Column col = target_tab.schema.at(col_index);
	if (col.indexed) {
		vector<FilePos> retpos = rangeFindInBPTree_(target_tab.bptree[key_col],col,first,last);
		for (FilePos &x : retpos)
			retval.push_back(RecordHandle(tabname,x));
	} else
		assert(0); // no trolling me, please don't rangeQuery on unindexed column
	return retval;
}

void NaiveDB::modify(RecordHandle handle, const string &colname, DBData val) {
	Table &target_tab = tables_.at(handle.tabname);
	int col_index = target_tab.colname_index.at(colname);
	Column col = target_tab.schema.at(col_index);
	target_tab.fileptr->seekp(handle.filepos + col.offset);
	assert(val.type == col.type);
	switch (val.type) {
	case DBType::BOOLEAN:
		binary_write(*target_tab.fileptr, val.boolean);
		break;
	case DBType::INT32:
		binary_write(*target_tab.fileptr, val.int32);
		break;
	case DBType::INT64:
		binary_write(*target_tab.fileptr, val.int64);
		break;
	case DBType::STRING:
		binary_write_s(*target_tab.fileptr, val.str, col.length);
		break;
	default:
		assert(0);
	}

	if (col.indexed)
		assert(0); // muhahahaha
}

NaiveDB::~NaiveDB() {
	for (pair<std::string,Table> x : tables_) {
		x.second.fileptr->close();
		delete x.second.fileptr;
		for (Column &col : x.second.schema)
			if (col.indexed)
				deleteBPTree_(x.second.bptree[col.name],col);
	}
}
