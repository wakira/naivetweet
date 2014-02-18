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

namespace DatFile {
// File layout definition

const FilePos kPidPos = 0;
const FilePos kFlHeadPos = 8;

int64_t increasePrimaryId(std::fstream &stream);
int64_t getPrimaryId(std::istream &is);

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
