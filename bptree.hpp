/*
 * B+Tree for implementing InnoDB
 * ----------------
 *
 */
#ifndef BPTREE_HPP
#define BPTREE_HPP

#include <stack>
#include <vector>
#include <fstream>
#include <string>
#include <cstring>
#include <cassert>
#include "diskfile.h"
#include "kikutil.h"

#ifndef NDEBUG
#include <iostream>
#endif

const size_t kBlockSize = 4096;

// The BPTree class
// Stores on disk file
// Leafs are linked
template <typename KeyType, typename ValType>
class BPTree {
private:
	// Order-related and constants for implementation

	static const size_t kBPOrder = 6; // FIXME this value should be calculated from BlockSize
	static const size_t kInnerSlotMinChildren = (kBPOrder + 1)/2;
	static const size_t kLeafSlotMin = (kBPOrder)/2;
	static const size_t kInnerNodePadding = 0; // FIXME
	static const size_t kLeafPadding = 0; // FIXME
public:
	// Base class for all nodes
	struct Node {
		// Number of KEYS in this node
		// The number of children is slotuse+1
		short slotuse;
		char nodetype;

		virtual bool isFull() {
			return slotuse == kBPOrder - 1;
		}

		virtual ~Node() {}
	};

	// Contains only array of data values
	struct Leaf : public Node {
		KeyType keys[kBPOrder - 1];
		ValType data[kBPOrder - 1];
		bool overflowptr[kBPOrder - 1];
		// Leafs are linked
		FilePos next_leaf;
		~Leaf() {}
	};

	struct InnerNode : public Node {
		KeyType keys[kBPOrder - 1];
		FilePos children[kBPOrder]; // Meaningless on leaf node
		~InnerNode() {}
	};

private:
	// Private data members

	Node *root_;

	FilePos rootpos_;

	std::string filename_;
	// Private helper member functions

	void insert_in_parent_(std::fstream &stream, std::stack<FilePos> parentpos, KeyType newkey, FilePos newnode_pos);

	Node* load_node_(std::fstream &stream, FilePos nodepos) const;

	void load_root_node_(std::fstream &stream);

	void write_node_(std::fstream &stream, FilePos nodepos, Node* p);

	void create_empty_tree_(std::fstream &stream);

	void create_new_root_(std::fstream &stream, KeyType newkey, FilePos lptr, FilePos rptr);

	template <typename NodeWithKeys>
	size_t find_lower_(NodeWithKeys *p, const KeyType &key) const;

	// return true on success, fail if the leaf node is full
	bool insert_in_leaf_(std::fstream &stream, Leaf *leaf_node, const KeyType &key, const ValType &value);
public:
	// Public methods

	BPTree(const std::string &filename);

	~BPTree() {} // FIXME default destructor

	std::vector<ValType> find(const KeyType &key) const;

	void insert(const KeyType &key, const ValType &value);

	bool erase(const KeyType &key);

	bool modify(const KeyType &key, const ValType &new_value);
};

// Implementations of class BPTree

template <typename KeyType, typename ValType>
BPTree<KeyType,ValType>::
		BPTree(const std::string &filename) : filename_(filename)
{

	// ASSERT
	static_assert(sizeof(ValType) >= sizeof(FilePos),"ValType too short!");

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
	// DEBUG ASSERT
	{
		int32_t keysize, valsize, blocksize;
		getFromPos(file,8,keysize);
		getFromPos(file,12,valsize);
		getFromPos(file,16,blocksize);
		assert(keysize == sizeof(KeyType));
		assert(valsize == sizeof(ValType));
		assert(blocksize == kBlockSize);
	}

	load_root_node_(file);
	file.close();
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		load_root_node_(std::fstream &stream) {

	FilePos rootpos;
	getFromPos(stream,IdxFile::kRootPointerPos,rootpos);
	rootpos_ = rootpos;
	root_ = load_node_(stream,rootpos);
}

template <typename KeyType, typename ValType>
typename BPTree<KeyType,ValType>::Node* BPTree<KeyType,ValType>::
		load_node_(std::fstream &stream, FilePos nodepos) const {

	Node *node;
	// the first byte determines if node type is a leaf
	char byte;
	getFromPos(stream,nodepos,byte);
	switch (byte) {
	case IdxFile::LEAF:
	case IdxFile::OVF: {
		node = new Leaf();
		Leaf *leaf = static_cast<Leaf*>(node);
		binary_read(stream,node->slotuse);
		for (size_t i = 0; i != kBPOrder - 1; ++i)
			binary_read(stream,leaf->keys[i]);
		for (size_t i = 0; i != kBPOrder - 1; ++i)
			binary_read(stream,leaf->data[i]);
		binary_read(stream,leaf->next_leaf);
		break;
	}
	default: {
		node = new InnerNode();
		InnerNode *inner = static_cast<InnerNode*>(node);
		binary_read(stream,node->slotuse);
		for (size_t i = 0; i != kBPOrder - 1; ++i)
			binary_read(stream,inner->keys[i]);
		for (size_t i = 0; i != kBPOrder; ++i)
			binary_read(stream,inner->children[i]);
	}
	}
	node->nodetype = (IdxFile::NodeType)byte;
	return node;
}

template <typename KeyType, typename ValType>
std::vector<ValType> BPTree<KeyType,ValType>::
		find(const KeyType &key) const {

	std::vector<ValType> retval;
	std::fstream stream(filename_,std::ios::in | std::ios::out |
						std::ios::binary);
	Node *p = root_;
	// Locate leaf or overflow node
	while (p->nodetype == IdxFile::INNER) {
		InnerNode *inner_node = dynamic_cast<InnerNode*>(p);
		size_t next_child_index = find_lower_(inner_node, key);
		Node* newnode = load_node_(stream,inner_node->children[next_child_index]);
		if (p != root_)
			delete p;
		p = newnode;
	}
	Leaf *leaf_node = dynamic_cast<Leaf*>(p);

	// Find in that node
	// Search in inner node, find correct children and continue the recursion
	size_t data_index = find_lower_(leaf_node, key);
	// TODO process duplicate key
	retval.push_back(leaf_node->data[data_index]);
	if (p != root_)
		delete p;
	return retval;
}

template <typename KeyType, typename ValType>
template <typename NodeWithKeys>
size_t BPTree<KeyType,ValType>::
		find_lower_(NodeWithKeys *p, const KeyType &key) const {

	// Linear search inside a node to find the place that is just smaller than or equal to key
	// TODO If kOrder is too large, use binary search instead
	size_t i;
	for (i = 0; i != p->slotuse; ++i)
		if (p->keys[i] >= key)
			break;
	return i;
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		insert(const KeyType &key, const ValType &value) {
#ifndef NDEBUG
	std::cout << "insert:" << key << std::endl;
#endif

	std::fstream stream(filename_.c_str(),std::ios::in | std::ios::out |
						std::ios::binary);
	// parent_trace includes leaf node
	std::stack<FilePos> parent_trace;
	parent_trace.push(rootpos_);
	std::stack<size_t> index_trace;
	Node *p = root_;
	while (p->nodetype == IdxFile::INNER) {
		InnerNode *inner_node = dynamic_cast<InnerNode*>(p);
		size_t next_child_index = find_lower_(inner_node, key);
		Node* newnode = load_node_(stream,inner_node->children[next_child_index]);
		parent_trace.push(inner_node->children[next_child_index]);
		if (p != root_) {
			delete p;
		}
		p = newnode;
		index_trace.push(next_child_index);
	}
	Leaf *old_leaf = dynamic_cast<Leaf*>(p);
	FilePos nodepos = parent_trace.top();

	if (insert_in_leaf_(stream,old_leaf,key,value)) {
		write_node_(stream,nodepos,old_leaf);
		return;
	} else {
		// split current node
		size_t newval_pos = find_lower_(old_leaf, key);
		size_t mid_pos = (old_leaf->slotuse - 1)/2;
		KeyType midkey = old_leaf->keys[mid_pos]; // to be inserted to parent
		Leaf *new_leaf = new Leaf();
		new_leaf->nodetype = IdxFile::LEAF;
		if (newval_pos <= mid_pos) {
			// new data to be placed in old leaf
			// copy the larger part to the new leaf
			new_leaf->slotuse = old_leaf->slotuse/2;
			old_leaf->slotuse = (old_leaf->slotuse + 1)/2 + 1;
			size_t i;
			for (i = mid_pos + 1; i != kBPOrder - 1; ++i) {
				new_leaf->keys[i - mid_pos - 1] = old_leaf->keys[i];
				new_leaf->data[i - mid_pos - 1] = old_leaf->data[i];
			}
			array_move(old_leaf->keys,kBPOrder - 1,newval_pos,1);
			array_move(old_leaf->data,kBPOrder - 1,newval_pos,1);
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
			for (; i != kBPOrder - 1; ++i) {
				new_leaf->keys[i - mid_pos] = old_leaf->keys[i];
				new_leaf->data[i - mid_pos] = old_leaf->data[i];
			}
		}
		// write old leaf and new leaf
		FilePos newnode_pos = IdxFile::consumeFreeSpace(stream);
		old_leaf->next_leaf = newnode_pos;
		write_node_(stream,nodepos,old_leaf);
		write_node_(stream,newnode_pos,new_leaf);
		delete new_leaf;
		if (old_leaf == root_) {
			create_new_root_(stream,midkey,nodepos,newnode_pos);
			delete old_leaf;
		} else {
			delete old_leaf;
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
		// TODO duplicate key

		return true;
	} else if (leaf_node->isFull()) {
		return false; // need split
	} else {
		// normal situation
		// move data backward
		array_move(leaf_node->keys,kBPOrder - 1,i,1);
		array_move(leaf_node->data,kBPOrder - 1,i,1);
		// insert
		leaf_node->keys[i] = key;
		leaf_node->data[i] = value;
		leaf_node->slotuse += 1;
		return true;
	}
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		write_node_(std::fstream &stream, FilePos nodepos, Node *p) {

	char byte = p->nodetype;
	writeToPos(stream,nodepos,byte);
	binary_write(stream,p->slotuse);
	switch (p->nodetype) {
	case IdxFile::INNER:
	case IdxFile::SINGLE: {
		InnerNode* inner = dynamic_cast<InnerNode*>(p);
		for (size_t i = 0; i != kBPOrder - 1; ++i)
			binary_write(stream,inner->keys[i]);
		for (size_t i = 0; i != kBPOrder; ++i)
			binary_write(stream,inner->children[i]);
		// padding
		byte = 0;
		for (size_t i = 0; i != kInnerNodePadding; ++i)
			binary_write(stream,byte);
		break;
	}
	default: {
		Leaf* leaf = dynamic_cast<Leaf*>(p);
		for (size_t i = 0; i != kBPOrder - 1; ++i)
			binary_write(stream,leaf->keys[i]);
		for (size_t i = 0; i != kBPOrder - 1; ++i)
			binary_write(stream, leaf->data[i]);
		binary_write(stream,leaf->next_leaf);
		// padding
		byte = 0;
		for (size_t i = 0; i != kLeafPadding; ++i)
			binary_write(stream,byte);
		break;
	}
	}
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		insert_in_parent_(std::fstream &stream, std::stack<FilePos> parentpos, KeyType newkey, FilePos newnode_pos) {
#ifndef NDEBUG
	std::cout << "insert_in_parent_" << std::endl;
#endif

	FilePos nodepos = parentpos.top();
	InnerNode *p;
	if (nodepos == rootpos_)
		p = dynamic_cast<InnerNode*>(root_);
	else
		p = dynamic_cast<InnerNode*>(load_node_(stream, nodepos));
	if (p->isFull()) {
		// split innernode
		size_t newval_pos = find_lower_(p, newkey);
		size_t mid_pos = (p->slotuse - 1)/2;
		KeyType midkey = p->keys[mid_pos]; // to be inserted to parent
		InnerNode *new_inner = new InnerNode();
		new_inner->nodetype = IdxFile::INNER;
		if (newval_pos <= mid_pos) {
			// new data to be placed in old node
			// copy the larger part to the new node
			new_inner->slotuse = p->slotuse/2;
			p->slotuse = (p->slotuse + 1)/2 + 1;
			size_t i;
			for (i = mid_pos + 1; i != kBPOrder - 1; ++i) {
				new_inner->keys[i - mid_pos - 1] = p->keys[i];
				new_inner->children[i - mid_pos - 1] = p->children[i];
			}
			new_inner->children[i - mid_pos - 1] = p->children[i];

			array_move(p->keys,kBPOrder - 1,newval_pos,1);
			array_move(p->children,kBPOrder - 1,newval_pos,1);
			p->keys[newval_pos] = newkey;
			p->children[newval_pos + 1] = newnode_pos;

		} else {
			// new data to be placed in new node
			new_inner->slotuse = p->slotuse/2 + 1;
			p->slotuse = (p->slotuse + 1)/2;
			size_t i;
			for (i = mid_pos + 1; i != kBPOrder - 1; ++i) {
				new_inner->keys[i - mid_pos - 1] = p->keys[i];
				new_inner->children[i - mid_pos - 1] = p->children[i];
			}
			new_inner->children[i - mid_pos - 1] = p->children[i];
			array_move(new_inner->keys,kBPOrder - 1,newval_pos - mid_pos - 1,1);
			array_move(new_inner->children,kBPOrder - 1,newval_pos - mid_pos - 1,1);
			new_inner->keys[newval_pos - mid_pos - 1] = newkey;
			new_inner->children[newval_pos - mid_pos] = newnode_pos;
		}
		// write old node and new node
		FilePos newinner_pos = IdxFile::consumeFreeSpace(stream);
		write_node_(stream,nodepos,p);
		write_node_(stream,newinner_pos,new_inner);
		delete new_inner;
		if (p == root_) {
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

	if (p != root_)
		delete p;
}

template <typename KeyType, typename ValType>
void BPTree<KeyType,ValType>::
		create_new_root_(std::fstream &stream, KeyType newkey, FilePos lptr, FilePos rptr) {
#ifndef NDEBUG
	std::cout << "create_new_root_" << std::endl;
#endif

	InnerNode *newroot = new InnerNode();
	newroot->slotuse = 1;
	newroot->nodetype = IdxFile::INNER;
	newroot->keys[0] = newkey;
	newroot->children[0] = lptr;
	newroot->children[1] = rptr;
	FilePos rootpos = IdxFile::consumeFreeSpace(stream);
	write_node_(stream,rootpos,newroot);
	writeToPos(stream,IdxFile::kRootPointerPos,rootpos);
	root_ = newroot;
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
	Leaf* root = new Leaf();
	root->nodetype = IdxFile::LEAF;
	root->slotuse = 0;
	write_node_(stream,4096,root);
	delete root;
}

#endif // BPTREE_HPP
