/*
 * B+Tree for implementing InnoDB
 * ----------------
 *
 */
#ifndef BPTREE_HPP
#define BPTREE_HPP

#include <algorithm>
#include <stack>
#include <vector>
#include <fstream>
#include <string>
#include <map>
#include <cstring>
#include <cassert>
#include "diskfile.h"
#include "kikutil.h"

// NOTE if your change kBlockSize, change kMaxBPOrder correspondently
const size_t kBlockSize = 4096;

// The BPTree class
// Stores on disk file
// Leafs are linked
template <typename KeyType, typename ValType>
class BPTree {
public:
	// Order-related configuration

	static const size_t kMaxBPOrder = 320;
	size_t BPOrder;
	size_t InnerNodePadding;
	size_t LeafPadding;
private:
	size_t keysize_;
	size_t valsize_;

	// Cache configuration
	static const size_t kMaxCachedNode = 10000;
public:
	// Base class for all nodes
	struct Node {
		// Number of KEYS in this node
		// The number of children is slotuse+1
		short slotuse;
		char nodetype;

		Node(const BPTree<KeyType,ValType> *context) : context_(context) {}

		virtual ~Node() {}
	private:
		const BPTree<KeyType,ValType> *context_;
	public:
		bool isFull() {
			return slotuse == context_->BPOrder - 1;
		}
	};

	// Contains only array of data values
	struct Leaf : public Node {
		KeyType keys[kMaxBPOrder];
		ValType data[kMaxBPOrder];
		char overflowptr[kMaxBPOrder];
		// Leafs are linked
		FilePos next_leaf;
		Leaf(const BPTree<KeyType,ValType> *context) : Node(context) {}
		~Leaf() {}
	};

	struct InnerNode : public Node {
		KeyType keys[kMaxBPOrder];
		FilePos children[kMaxBPOrder]; // Meaningless on leaf node
		InnerNode(const BPTree<KeyType,ValType> *context) : Node(context) {}
		~InnerNode() {}
	};

private:
	// Private data members

	FilePos rootpos_;

	std::string filename_;

	std::map<FilePos, Node*> cache_;
	// Private helper member functions

	void swapOutAllCache_(std::fstream &stream);
	void insert_in_parent_(std::fstream &stream, std::stack<FilePos> parentpos, KeyType newkey, FilePos newnode_pos);
	Node* load_node_(std::fstream &stream, FilePos nodepos);
	Node* load_node_from_disk_(std::fstream &stream, FilePos nodepos) const;
	void load_root_node_(std::fstream &stream);
	void write_node_(std::fstream &stream, FilePos nodepos, Node* p);
	void write_node_to_disk_(std::fstream &stream, FilePos nodepos, Node* p);
	void create_empty_tree_(std::fstream &stream);
	void create_new_root_(std::fstream &stream, KeyType newkey, FilePos lptr, FilePos rptr);
	void initConfiguration_(size_t keysize, size_t valsize);
	template <typename NodeWithKeys>
	size_t find_lower_(NodeWithKeys *p, const KeyType &key) const;

	// return true on success, fail if the leaf node is full
	bool insert_in_leaf_(std::fstream &stream, Leaf *leaf_node, const KeyType &key, const ValType &value);
public:
	// Public methods

	BPTree(const std::string &filename, size_t keysize = 0, size_t valsize = 0);

	~BPTree();

	std::vector<ValType> find(const KeyType &key);

	void insert(const KeyType &key, const ValType &value);

	bool erase(const KeyType &key);

	bool modify(const KeyType &key, const ValType &new_value);

	std::vector<ValType> rangeFind(const KeyType &first,const KeyType &last);
};

// Implementations of class BPTree

template <typename KeyType, typename ValType>
BPTree<KeyType,ValType>::~BPTree() {

	std::fstream file(filename_.c_str(), std::ios::in | std::ios::out |
					  std::ios::binary);
	swapOutAllCache_(file);
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		initConfiguration_(size_t keysize, size_t valsize) {

	if (keysize == 0)
		keysize = sizeof(KeyType);
	if (valsize == 0)
		valsize = sizeof(ValType);
	keysize_ = keysize;
	valsize_ = valsize;
	BPOrder = (kBlockSize - 2 + keysize)/(keysize + valsize + 1);
	InnerNodePadding = kBlockSize -
			(BPOrder-1)*keysize - BPOrder*sizeof(FilePos) - 3;
	LeafPadding = kBlockSize - (BPOrder-1)*keysize - (BPOrder-1)*valsize -
			sizeof(FilePos) - (BPOrder-1) - 3;
}

template <typename KeyType, typename ValType>
BPTree<KeyType,ValType>::
		BPTree(const std::string &filename,size_t keysize, size_t valsize)
			: filename_(filename)
{

	// ASSERT
	static_assert(sizeof(ValType) >= sizeof(FilePos),"ValType too short!");

	initConfiguration_(keysize,valsize);

	if (!fileExists(filename.c_str())) {
		// empty file
		std::fstream file(filename.c_str(), std::ios::out |
					  std::ios::binary);
		create_empty_tree_(file);
		file.close();
	}

	// load meta
	std::fstream file(filename.c_str(), std::ios::in | std::ios::out |
					  std::ios::binary);
	load_root_node_(file);
	file.close();
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		load_root_node_(std::fstream &stream) {

	FilePos rootpos;
	getFromPos(stream,IdxFile::kRootPointerPos,rootpos);
	rootpos_ = rootpos;
}

template <typename KeyType, typename ValType>
typename BPTree<KeyType,ValType>::Node* BPTree<KeyType,ValType>::
		load_node_from_disk_(std::fstream &stream, FilePos nodepos) const {

	Node *node;
	// the first byte determines if node type is a leaf
	char byte;
	getFromPos(stream,nodepos,byte);
	switch (byte) {
	case IdxFile::LEAF:
	case IdxFile::OVF: {
		node = new Leaf(this);
		Leaf *leaf = static_cast<Leaf*>(node);
		binary_read(stream,node->slotuse);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_read(stream,leaf->keys[i]);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_read(stream,leaf->data[i]);
		binary_read(stream,leaf->next_leaf);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_read(stream,leaf->overflowptr[i]);
		break;
	}
	default: {
		node = new InnerNode(this);
		InnerNode *inner = static_cast<InnerNode*>(node);
		binary_read(stream,node->slotuse);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_read(stream,inner->keys[i]);
		for (size_t i = 0; i != BPOrder; ++i)
			binary_read(stream,inner->children[i]);
	}
	}
	node->nodetype = (IdxFile::NodeType)byte;
	return node;
}

// Specialization for string and FilePos
template <>
inline typename BPTree<std::string,FilePos>::Node* BPTree<std::string,FilePos>::
		load_node_from_disk_(std::fstream &stream, FilePos nodepos) const {

	Node *node;
	// the first byte determines if node type is a leaf
	char byte;
	getFromPos(stream,nodepos,byte);
	switch (byte) {
	case IdxFile::LEAF:
	case IdxFile::OVF: {
		node = new Leaf(this);
		Leaf *leaf = static_cast<Leaf*>(node);
		binary_read(stream,node->slotuse);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_read_s(stream,leaf->keys[i],keysize_);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_read(stream,leaf->data[i]);
		binary_read(stream,leaf->next_leaf);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_read(stream,leaf->overflowptr[i]);
		break;
	}
	default: {
		node = new InnerNode(this);
		InnerNode *inner = static_cast<InnerNode*>(node);
		binary_read(stream,node->slotuse);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_read_s(stream,inner->keys[i],keysize_);
		for (size_t i = 0; i != BPOrder; ++i)
			binary_read(stream,inner->children[i]);
	}
	}
	node->nodetype = (IdxFile::NodeType)byte;
	return node;
}

template <typename KeyType, typename ValType>
typename BPTree<KeyType,ValType>::Node* BPTree<KeyType,ValType>::
		load_node_(std::fstream &stream, FilePos nodepos) {

	// found in cache
	if (cache_.count(nodepos) > 0)
		return cache_[nodepos];
	if (cache_.size() == kMaxCachedNode) {
		// swap out all cache
		swapOutAllCache_(stream);
	}
	cache_[nodepos] = load_node_from_disk_(stream,nodepos);
	return cache_[nodepos];
}

template <typename KeyType, typename ValType>
std::vector<ValType> BPTree<KeyType,ValType>::
		find(const KeyType &key) {

	std::vector<ValType> retval;
	std::fstream stream(filename_,std::ios::in | std::ios::out |
						std::ios::binary);
	Node *p = load_node_(stream,rootpos_);
	// Locate leaf or overflow node
	while (p->nodetype == IdxFile::INNER) {
		InnerNode *inner_node = static_cast<InnerNode*>(p);
		size_t next_child_index = find_lower_(inner_node, key);
		Node* newnode = load_node_(stream,inner_node->children[next_child_index]);
		p = newnode;
	}
	Leaf *leaf_node = static_cast<Leaf*>(p);

	// Find in that node
	// Search in inner node, find correct children and continue the recursion
	size_t data_index = find_lower_(leaf_node, key);
	if (leaf_node->overflowptr[data_index] == true && leaf_node->keys[data_index] == key) {
		// process duplicate key
		Leaf *overflow = static_cast<Leaf*>(
					load_node_(stream,leaf_node->data[data_index]));
		while (true) {
			for (size_t i = 0; i != overflow->slotuse; ++i)
				retval.push_back(overflow->data[i]);
			if (overflow->next_leaf == 0)
				break;
			else {
				Leaf *next_overflow = static_cast<Leaf*>(
							load_node_(stream,overflow->next_leaf));
				//delete overflow;
				overflow = next_overflow;
			}
		}
		//delete overflow;
	} else if (leaf_node->keys[data_index] == key)
		// normal situation, determine if key is found and then add the key
		retval.push_back(leaf_node->data[data_index]);
	return retval;
}

template <typename KeyType, typename ValType>
template <typename NodeWithKeys>
size_t BPTree<KeyType,ValType>::
		find_lower_(NodeWithKeys *p, const KeyType &key) const {

	// Linear search version
	//size_t i;
	//for (i = 0; i != p->slotuse; ++i)
	//	if (p->keys[i] >= key)
	//		break;
	//return i;

	// Binary search inside a node to find the place that is just smaller than or equal to key
	return std::lower_bound(p->keys,p->keys + p->slotuse,key) - p->keys;
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		insert(const KeyType &key, const ValType &value) {

	std::fstream stream(filename_.c_str(),std::ios::in | std::ios::out |
						std::ios::binary);
	// parent_trace includes leaf node
	std::stack<FilePos> parent_trace;
	parent_trace.push(rootpos_);
	std::stack<size_t> index_trace;
	Node *p = load_node_(stream,rootpos_);
	while (p->nodetype == IdxFile::INNER) {
		InnerNode *inner_node = static_cast<InnerNode*>(p);
		size_t next_child_index = find_lower_(inner_node, key);
		Node* newnode = load_node_(stream,inner_node->children[next_child_index]);
		parent_trace.push(inner_node->children[next_child_index]);
		p = newnode;
		index_trace.push(next_child_index);
	}
	Leaf *old_leaf = static_cast<Leaf*>(p);
	FilePos nodepos = parent_trace.top();

	if (insert_in_leaf_(stream,old_leaf,key,value)) {
		write_node_(stream,nodepos,old_leaf);
		return;
	} else {
		// split current node
		size_t newval_pos = find_lower_(old_leaf, key);
		size_t mid_pos = (old_leaf->slotuse - 1)/2;
		KeyType midkey = old_leaf->keys[mid_pos]; // to be inserted to parent
		Leaf *new_leaf = new Leaf(this);
		new_leaf->nodetype = IdxFile::LEAF;
		if (newval_pos <= mid_pos) {
			// new data to be placed in old leaf
			// copy the larger part to the new leaf
			new_leaf->slotuse = old_leaf->slotuse/2;
			old_leaf->slotuse = (old_leaf->slotuse + 1)/2 + 1;
			size_t i;
			for (i = mid_pos + 1; i != BPOrder - 1; ++i) {
				new_leaf->keys[i - mid_pos - 1] = old_leaf->keys[i];
				new_leaf->data[i - mid_pos - 1] = old_leaf->data[i];
			}
			array_move(old_leaf->keys,BPOrder - 1,newval_pos,1);
			array_move(old_leaf->data,BPOrder - 1,newval_pos,1);
			old_leaf->keys[newval_pos] = key;
			old_leaf->data[newval_pos] = value;
		} else {
			// new data to be placed in new leaf
			new_leaf->slotuse = old_leaf->slotuse/2 + 1;
			old_leaf->slotuse = (old_leaf->slotuse + 1)/2;
			size_t i;
			for (i = mid_pos + 1; i != newval_pos; ++i) {
				new_leaf->keys[i - mid_pos - 1] = old_leaf->keys[i];
				new_leaf->data[i - mid_pos - 1] = old_leaf->data[i];
			}
			new_leaf->keys[i - mid_pos - 1] = key;
			new_leaf->data[i - mid_pos - 1] = value;
			for (; i != BPOrder - 1; ++i) {
				new_leaf->keys[i - mid_pos] = old_leaf->keys[i];
				new_leaf->data[i - mid_pos] = old_leaf->data[i];
			}
		}
		// write old leaf and new leaf
		FilePos newnode_pos = IdxFile::consumeFreeSpace(stream);
		new_leaf->next_leaf = old_leaf->next_leaf;
		old_leaf->next_leaf = newnode_pos;
		write_node_(stream,nodepos,old_leaf);
		write_node_(stream,newnode_pos,new_leaf);
		//delete new_leaf;
		if (nodepos == rootpos_) {
			create_new_root_(stream,midkey,nodepos,newnode_pos);
		} else {
			parent_trace.pop(); // remove leaf node from parent_trace
			insert_in_parent_(stream,parent_trace,midkey,newnode_pos);
		}

	}
}

template <typename KeyType, typename ValType>
bool BPTree<KeyType,ValType>::
		insert_in_leaf_(std::fstream &stream,Leaf *leaf_node, const KeyType &key, const ValType &value) {

	bool duplicate = false;
	size_t i;
	for (i = 0; i != leaf_node->slotuse; ++i) {
		if (leaf_node->keys[i] == key) {
			duplicate = true;
			break;
		}
		if (leaf_node->keys[i] > key)
			break;
	}
	if (duplicate) {
		// duplicate key
		if (leaf_node->overflowptr[i] == true) {
			// already has overflow node
			// locate the last one and insert in it
			Leaf *overflow = static_cast<Leaf*>(
						load_node_(stream,leaf_node->data[i]));
			FilePos overflow_pos = leaf_node->data[i];
			while (overflow->next_leaf != 0) {
				overflow_pos = overflow->next_leaf;
				Leaf *next_overflow = static_cast<Leaf*>(
							load_node_(stream,overflow->next_leaf));
				//delete overflow;
				overflow = next_overflow;
			}
			if (overflow->isFull()) {
				// require new node
				Leaf *new_overflow = new Leaf(this);
				new_overflow->nodetype = IdxFile::OVF;
				new_overflow->slotuse = 1;
				new_overflow->keys[0] = key;
				new_overflow->data[0] = value;
				FilePos new_overflow_pos = IdxFile::consumeFreeSpace(stream);
				overflow->next_leaf = new_overflow_pos;
				write_node_(stream,new_overflow_pos,new_overflow);
				//delete new_overflow;
			} else {
				overflow->keys[overflow->slotuse] = key;
				overflow->data[overflow->slotuse] = value;
				overflow->slotuse++;
			}
			write_node_(stream,overflow_pos,overflow); // write changes back
			//delete overflow;
		} else {
			// need to create overflow node
			Leaf *new_overflow = new Leaf(this);
			new_overflow->nodetype = IdxFile::OVF;
			new_overflow->slotuse = 2;
			new_overflow->keys[1] = key;
			new_overflow->data[1] = value;
			new_overflow->keys[0] = key;
			new_overflow->data[0] = leaf_node->data[i];
			FilePos new_overflow_pos = IdxFile::consumeFreeSpace(stream);
			leaf_node->data[i] = new_overflow_pos;
			leaf_node->overflowptr[i] = true;
			write_node_(stream,new_overflow_pos,new_overflow);
			//delete new_overflow;
		}
		return true;
	} else if (leaf_node->isFull()) {
		return false; // need split
	} else {
		// normal situation
		// move data backward
		array_move(leaf_node->keys,BPOrder - 1,i,1);
		array_move(leaf_node->data,BPOrder - 1,i,1);
		// insert
		leaf_node->keys[i] = key;
		leaf_node->data[i] = value;
		leaf_node->slotuse += 1;
		return true;
	}
}

// Specialization for string and filepos
template <>
inline void BPTree<std::string,FilePos>::
		write_node_to_disk_(std::fstream &stream, FilePos nodepos, Node *p) {

	char byte = p->nodetype;
	writeToPos(stream,nodepos,byte);
	binary_write(stream,p->slotuse);
	switch (p->nodetype) {
	case IdxFile::INNER:
	case IdxFile::SINGLE: {
		InnerNode* inner = static_cast<InnerNode*>(p);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_write_s(stream,inner->keys[i],keysize_);
		for (size_t i = 0; i != BPOrder; ++i)
			binary_write(stream,inner->children[i]);
		// padding
		byte = 0;
		for (size_t i = 0; i != InnerNodePadding; ++i)
			binary_write(stream,byte);
		break;
	}
	default: {
		Leaf* leaf = static_cast<Leaf*>(p);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_write_s(stream,leaf->keys[i],keysize_);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_write(stream, leaf->data[i]);
		binary_write(stream,leaf->next_leaf);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_write(stream, leaf->overflowptr[i]);
		// padding
		byte = 0;
		for (size_t i = 0; i != LeafPadding; ++i)
			binary_write(stream,byte);
		break;
	}
	}
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		write_node_to_disk_(std::fstream &stream, FilePos nodepos, Node *p) {

	char byte = p->nodetype;
	writeToPos(stream,nodepos,byte);
	binary_write(stream,p->slotuse);
	switch (p->nodetype) {
	case IdxFile::INNER:
	case IdxFile::SINGLE: {
		InnerNode *inner = static_cast<InnerNode*>(p);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_write(stream,inner->keys[i]);
		for (size_t i = 0; i != BPOrder; ++i)
			binary_write(stream,inner->children[i]);
		// padding
		byte = 0;
		for (size_t i = 0; i != InnerNodePadding; ++i)
			binary_write(stream,byte);
		break;
	}
	default: {
		Leaf *leaf = static_cast<Leaf*>(p);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_write(stream,leaf->keys[i]);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_write(stream, leaf->data[i]);
		binary_write(stream,leaf->next_leaf);
		for (size_t i = 0; i != BPOrder - 1; ++i)
			binary_write(stream, leaf->overflowptr[i]);
		// padding
		byte = 0;
		for (size_t i = 0; i != LeafPadding; ++i)
			binary_write(stream,byte);
		break;
	}
	}
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		write_node_(std::fstream &stream, FilePos nodepos, Node *p) {

	if (cache_.count(nodepos) > 0) {
		return; // no need to write to disk
	} else
		write_node_to_disk_(stream,nodepos,p);
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		insert_in_parent_(std::fstream &stream, std::stack<FilePos> parentpos, KeyType newkey, FilePos newnode_pos) {

	FilePos nodepos = parentpos.top();
	InnerNode *p;
	p = static_cast<InnerNode*>(load_node_(stream, nodepos));
	if (p->isFull()) {
		// split innernode
		size_t newval_pos = find_lower_(p, newkey);
		size_t mid_pos = (p->slotuse - 1)/2;
		KeyType midkey = p->keys[mid_pos]; // to be inserted to parent
		InnerNode *new_inner = new InnerNode(this);
		new_inner->nodetype = IdxFile::INNER;
		if (newval_pos <= mid_pos) {
			// new data to be placed in old node
			// copy the larger part to the new node
			new_inner->slotuse = p->slotuse/2;
			p->slotuse = (p->slotuse + 1)/2 + 1;
			size_t i;
			for (i = mid_pos + 1; i != BPOrder - 1; ++i) {
				new_inner->keys[i - mid_pos - 1] = p->keys[i];
				new_inner->children[i - mid_pos - 1] = p->children[i];
			}
			new_inner->children[i - mid_pos - 1] = p->children[i];

			array_move(p->keys,BPOrder - 1,newval_pos,1);
			array_move(p->children,BPOrder - 1,newval_pos,1);
			p->keys[newval_pos] = newkey;
			p->children[newval_pos + 1] = newnode_pos;

		} else {
			// new data to be placed in new node
			new_inner->slotuse = p->slotuse/2 + 1;
			p->slotuse = (p->slotuse + 1)/2;
			size_t i;
			for (i = mid_pos + 1; i != BPOrder - 1; ++i) {
				new_inner->keys[i - mid_pos - 1] = p->keys[i];
				new_inner->children[i - mid_pos - 1] = p->children[i];
			}
			new_inner->children[i - mid_pos - 1] = p->children[i];
			array_move(new_inner->keys,BPOrder - 1,newval_pos - mid_pos - 1,1);
			array_move(new_inner->children,BPOrder - 1,newval_pos - mid_pos - 1,1);
			new_inner->keys[newval_pos - mid_pos - 1] = newkey;
			new_inner->children[newval_pos - mid_pos] = newnode_pos;
		}
		// write old node and new node
		FilePos newinner_pos = IdxFile::consumeFreeSpace(stream);
		write_node_(stream,nodepos,p);
		write_node_(stream,newinner_pos,new_inner);
		//delete new_inner;
		if (nodepos == rootpos_) {
			create_new_root_(stream,midkey,nodepos,newinner_pos);
		} else {
			parentpos.pop(); // remove this node from parent_trace
			insert_in_parent_(stream,parentpos,midkey,newinner_pos);
		}

	} else {
		size_t newpos = find_lower_(p,newkey);
		p->slotuse += 1;
		array_move(p->keys,p->slotuse,newpos,1);
		array_move(p->children,p->slotuse + 1,newpos,1);
		p->keys[newpos] = newkey;
		p->children[newpos + 1] = newnode_pos;
		write_node_(stream,nodepos,p);
	}

}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		create_new_root_(std::fstream &stream, KeyType newkey, FilePos lptr, FilePos rptr) {

	InnerNode *newroot = new InnerNode(this);
	newroot->slotuse = 1;
	newroot->nodetype = IdxFile::INNER;
	newroot->keys[0] = newkey;
	newroot->children[0] = lptr;
	newroot->children[1] = rptr;
	FilePos rootpos = IdxFile::consumeFreeSpace(stream);
	write_node_(stream,rootpos,newroot);
	writeToPos(stream,IdxFile::kRootPointerPos,rootpos);
	rootpos_ = rootpos;
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		create_empty_tree_(std::fstream &stream) {

	stream.seekp(0);
	FilePos pos = 0;
	binary_write(stream,pos);
	int32_t size = sizeof(KeyType);
	binary_write(stream,size);
	size = sizeof(ValType);
	binary_write(stream,size);
	size = kBlockSize;
	binary_write(stream,size);
	// root node pointer
	pos = 4096;
	binary_write(stream,pos);
	Leaf* root = new Leaf(this);
	root->nodetype = IdxFile::LEAF;
	root->slotuse = 0;
	write_node_(stream,4096,root);
	delete root;
}

template <typename KeyType, typename ValType>
std::vector<ValType> BPTree<KeyType,ValType>::
		rangeFind(const KeyType &first, const KeyType &last) {

	std::vector<ValType> retval;
	std::fstream stream(filename_,std::ios::in | std::ios::out |
						std::ios::binary);
	Node *p = load_node_(stream,rootpos_);
	// Locate leaf or overflow node
	while (p->nodetype == IdxFile::INNER) {
		InnerNode *inner_node = static_cast<InnerNode*>(p);
		size_t next_child_index = find_lower_(inner_node, first);
		Node* newnode = load_node_(stream,inner_node->children[next_child_index]);
		p = newnode;
	}
	Leaf *leaf_node = static_cast<Leaf*>(p);
	KeyType key = first;

	// Find in that node
	// Search in inner node, find correct children and continue the recursion
	size_t data_index = find_lower_(leaf_node, key);
	key = leaf_node->keys[data_index];
	while (key <= last) {
		if (leaf_node->overflowptr[data_index] && leaf_node->keys[data_index] == key) {
			// process duplicate key
			Leaf *overflow = static_cast<Leaf*>(
						load_node_(stream,leaf_node->data[data_index]));
			while (true) {
				for (size_t i = 0; i != overflow->slotuse; ++i)
					retval.push_back(overflow->data[i]);
				if (overflow->next_leaf == 0)
					break;
				else {
					Leaf *next_overflow = static_cast<Leaf*>(
								load_node_(stream,overflow->next_leaf));
					//delete overflow;
					overflow = next_overflow;
				}
			}
			//delete overflow;
		} else if (leaf_node->keys[data_index] == key) {
			// normal situation, determine if key is found and then add the key
			retval.push_back(leaf_node->data[data_index]);
		}

		// change leaf_node ptr and data_index
		if (data_index + 1 >= leaf_node->slotuse && leaf_node->next_leaf != 0) {
			Leaf *next_leaf = static_cast<Leaf*>(
						load_node_(stream,leaf_node->next_leaf));
			//delete leaf_node;
			leaf_node = next_leaf;
			p = next_leaf;
			data_index = 0;
			key = leaf_node->keys[data_index];
		} else if (data_index + 1 >= leaf_node->slotuse && leaf_node->next_leaf == 0) {
			break;
		} else {
			++data_index;
			key = leaf_node->keys[data_index];
		}
	}
	return retval;
}


template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
			swapOutAllCache_(std::fstream &stream) {

	for (auto &pair : cache_) {
		write_node_to_disk_(stream,pair.first,pair.second);
		delete pair.second;
	}
	cache_.clear();
}

#endif // BPTREE_HPP
