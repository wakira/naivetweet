#ifndef DISKFILE_H
#define DISKFILE_H

#include <fstream>
#include <string>
#include <cstdint>
#include "kikutil.h"

typedef int64_t FilePos;

template <typename T>
void writeToPos(std::ostream &ofs, FilePos pos, const T &data) {
	ofs.seekp(pos);
	binary_write(ofs, data);
}

template <typename T>
void getFromPos(std::istream &ifs, FilePos pos, T &data) {
	ifs.seekg(pos);
	binary_read(ifs, data);
}

// Special string read function
inline void binary_read_s(std::istream &ifs, std::string &str, size_t length) {
	str.clear();
	size_t count = 0;
	char byte;
	while ((byte = ifs.get()) != '\0') {
		str.push_back(byte);
		++count;
	}
	++count;
	for (; count < length; ++count)
		ifs.get();
}

// Special string write function
inline void binary_write_s(std::ostream &ofs, const std::string &str, size_t length) {
	size_t count;
	for (count = 0; count != str.length(); ++count)
		ofs.put(str[count]);
	for (; count < length; ++count)
		ofs.put('\0');
}

bool fileExists(const char* filename);

namespace DatFile {
// File layout definition

const FilePos kPidPos = 0;
const FilePos kFlHeadPos = 8;
const FilePos kRecordStartPos = 17;

int64_t increasePrimaryId(std::fstream &stream);
int64_t getPrimaryId(std::istream &is);
bool isRecordDeleted(std::istream &is, FilePos recordpos);

// consumeFreeSpace
// ----------------
// consume a piece of free space in free list (or append to the
// end of file, return position which is ready for r/w
//
FilePos consumeFreeSpace(std::fstream &stream);

}

namespace IdxFile {
// File layout definition

const FilePos kFlHeadPos = 0;
const FilePos kKeySizePos = 8;
const FilePos kValSizePos = 12;
const FilePos kBlockSizePos = 16;
const FilePos kRootPointerPos = 20;

enum NodeType { SINGLE = 0, INNER = 1, LEAF = 2, OVF = 3};

// consumeFreeSpace
// ----------------
// consume a piece of free space in free list (or append to the
// end of file, return position which is ready for r/w
//
FilePos consumeFreeSpace(std::fstream &stream);
}

#endif // DISKFILE_H
