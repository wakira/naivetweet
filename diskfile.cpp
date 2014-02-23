#include "diskfile.h"
#include <sys/stat.h>
using namespace std;

bool DatFile::isRecordDeleted(istream &is, FilePos recordpos) {
	char byte;
	getFromPos(is,recordpos - sizeof(char),byte);
	return byte == 1;
}

bool fileExists(const char *filename) {
	struct stat buf;
	if (stat(filename, &buf) != -1) {
		return true;
	}
	return false;
}

int64_t DatFile::increasePrimaryId(fstream &stream) {
	// increase the first 8 byte of table file
	int64_t pid;
	auto old_p = stream.tellp();
	auto old_g = stream.tellg();
	getFromPos(stream,DatFile::kPidPos,pid);
	writeToPos(stream,DatFile::kPidPos,pid + 1);
	stream.seekg(old_g);
	stream.seekp(old_g);
	return pid + 1;
}

int64_t DatFile::getPrimaryId(istream &is) {
	int64_t pid;
	auto old_g = is.tellg();
	getFromPos(is,DatFile::kPidPos,pid);
	is.seekg(old_g);
	return pid;
}

FilePos DatFile::consumeFreeSpace(fstream &stream) {
	FilePos next_flpos;
	getFromPos(stream,DatFile::kFlHeadPos,next_flpos); // get a chunk from the head of free list
	if (next_flpos == 0) {
		// no space in free list
		// return position pointing to file end
		stream.seekp(0, stream.end);
		// write meta info of deleted flag 0
		char bytedat = 0;
		binary_write(stream, bytedat);
	} else {
		FilePos current_chunk = next_flpos;
		// a free chunk is found
		// remove it from free list
		getFromPos(stream,next_flpos,next_flpos);
		writeToPos(stream,DatFile::kFlHeadPos,next_flpos);
		// set deleted flag false
		// write 0 to deleted flag
		char bytedat = 0;
		writeToPos(stream,current_chunk - sizeof(char),bytedat);
	}
	return stream.tellp();
}

FilePos IdxFile::consumeFreeSpace(fstream &stream) {
	FilePos next_flpos;
	getFromPos(stream,IdxFile::kFlHeadPos,next_flpos); // get a chunk from the head of free list
	if (next_flpos == 0) {
		// no space in free list
		// return position pointing to file end
		stream.seekp(0, stream.end);
	} else {
		FilePos current_chunk = next_flpos;
		// a free chunk is found
		// remove it from free list
		getFromPos(stream,next_flpos,next_flpos);
		writeToPos(stream,IdxFile::kFlHeadPos,next_flpos);
	}
	return stream.tellp();
}
