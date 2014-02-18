#ifndef KIKUTIL_H
#define KIKUTIL_H

/*
 * C++11 is required to use this utility
 * ----------------
 * Usage:
 * ----------------
 * DISALLOW_COPY_AND_ASSIGN(classname);
 * ----------------
 * ON_SCOPE_EXIT( function );
 * ----------------
 * class BitReader:
 * class BitWriter:
 */

#include <exception>
#include <functional>
#include <istream>
#include <ostream>
#include <cassert>

/* Macros */

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
TypeName(const TypeName&) = delete;               \
TypeName& operator=(const TypeName&) = delete

#define SCOPEGUARD_LINENAME_CAT(name, line) name##line
#define SCOPEGUARD_LINENAME(name, line) SCOPEGUARD_LINENAME_CAT(name, line)
#define ON_SCOPE_EXIT(callback) ScopeGuard SCOPEGUARD_LINENAME(EXIT, __LINE__)(callback)

/* Functions */
// Functions for binary input/ouput on fstream
template <typename T>
inline void binary_read(std::istream &ifs, T &data) {
	ifs.read(reinterpret_cast<char *>(&data), sizeof(T));
}

template <typename T>
inline void binary_write(std::ostream &ofs, const T &data) {
	ofs.write(reinterpret_cast<char const*>(&data), sizeof(T));
}

// Functions for array operation
template <typename T>
inline void array_move(T* array, size_t array_size, int beginpos, int move) {
	assert(move > 0);
	for (int i = array_size - 1; i >= beginpos + move; --i) {
		array[i] = array[i - move];
	}
}

/* Classes */

class ScopeGuard {
	DISALLOW_COPY_AND_ASSIGN(ScopeGuard);
public:
	explicit ScopeGuard(std::function<void()> onExitScope)
		: onExitScope_(onExitScope), dismissed_(false)
	{  }

	~ScopeGuard() {
		if (!dismissed_)
			onExitScope_();
	}

	void dismiss() {
		dismissed_ = true;
	}

private:
	std::function<void()> onExitScope_;
	bool dismissed_;
};

class BitWriter {
	DISALLOW_COPY_AND_ASSIGN(BitWriter);
private:
	char bslots_count_;
	char buffer_;
	std::ostream &os_;
public:
	void writeb(char bit) {
		if (bslots_count_ == 0)
			flush();
		--bslots_count_;
		buffer_ |= (bit&0x1) << bslots_count_;
	}

	void flush() {
		if (bslots_count_ == 8)
			return;
		bslots_count_ = 8;
		binary_write(os_, buffer_);
		buffer_ = 0;
	}

	// Constructor
	BitWriter(std::ostream &os) :
		bslots_count_(8) , buffer_(0), os_(os)
	{  }
};

class BitReader {
	DISALLOW_COPY_AND_ASSIGN(BitReader);
private:
	std::istream &is_;
	char buffer_;
	char remain_;

public:
	char getb() {
		if (remain_) {
			--remain_;
			return (buffer_ >> remain_ ) & 0x1;
		} else {
			binary_read(is_,buffer_);
			remain_ = 8; 
			return getb();
		}
	}
	// Constructor
	BitReader(std::istream &is) :
		is_(is), buffer_(0), remain_(0) 
			{  }
};

// Exception Definitions

#endif
