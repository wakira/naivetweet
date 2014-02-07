/*
 * B+Tree for implementing InnoDB
 * ----------------
 *
 */
#ifndef BPTREE_HPP
#define BPTREE_HPP

#include <stack>
#include <fstream>
#include <string>
#include <cstring>
#include <cassert>
#include "diskfile.h"
#include "kikutil.h"

// The BPTree class
// Stores on disk file
// Leafs are linked
template <typename KeyType, typename ValType, size_t BlockSize>
class BPTree {
private:
	// Order-related and constants for implementation

	static const size_t kBPOrder = 100; // FIXME this value should be calculated from BlockSize
	static const size_t kRootMinChildren = 2; // Don't change this or it's not a B+Tree
	static const size_t kInnerSlotMinChildren = (kBPOrder + 1)/2;
	static const size_t kLeafSlotMin = (kBPOrder)/2;
public:
	// Class definitions

	class NodeNotFound {}; // Throws in find()
	class DuplicateKey {};

	// Base class for all nodes
	struct Node {
		// Number of KEYS in this node
		// The number of children is slotuse+1
		size_t slotuse;
		IdxFile::NodeType nodetype;
	};

	// Contains only array of data values
	struct Leaf : public Node {
		KeyType keys[kBPOrder - 1];
		ValType data[kBPOrder - 1];
		// Leafs are linked
		FilePos next_leaf;
	};

	struct InnerNode : public Node {
		KeyType keys[kBPOrder - 1];
		FilePos children[kBPOrder]; // Meaningless on leaf node
	};

private:
	// Private data members

	std::fstream &file_;
	Node *root_;
	// Private helper member functions

	void load_root_node_(std::fstream &stream);

	void create_empty_tree_(std::fstream &stream); // TODO implement it

	template <typename NodeWithKeys>
	size_t find_lower_(NodeWithKeys *p, const KeyType &key);

	// return true on success, fail if the leaf node is full
	bool insert_in_leaf_(Leaf *leaf_node, const KeyType &key, const ValType &value);
public:
	// Public methods

	BPTree(const std::string &filename);

	~BPTree();

	ValType find(const KeyType &key) const throw(NodeNotFound);

	void insert(const KeyType &key, const ValType &value);

	bool erase(const KeyType &key);

	bool modify(const KeyType &key, const ValType &new_value);
};

// Implementations of class BPTree

template <typename KeyType, typename ValType, size_t BlockSize>
BPTree<KeyType,ValType,BlockSize>::
		BPTree(const std::string &filename) {

	// ASSERT
	static_assert(sizeof(ValType) >= sizeof(FilePos),"ValType too short!");

	// load meta
	std::fstream file(filename.c_str(), std::ios::binary | std::ios::end);
	if (file.tellg() == 0) {
		// empty file
		create_empty_tree_(file);
		return;
	}
	// DEBUG ASSERT
	{
		int32_t keysize, valsize, blocksize;
		getFromPos(stream,8,keysize);
		getFromPos(stream,12,valsize);
		getFromPos(stream,16,blocksize);
		assert(keysize == sizeof(KeyType));
		assert(valsize == sizeof(ValType));
		assert(blocksize == BlockSize);
	}

	load_root_node_(stream);
}

template <typename KeyType, typename ValType, size_t BlockSize>
ValType BPTree<KeyType,ValType,BlockSize>::
		load_root_node_(std::fstream &stream) {

	// get root node position
	FilePos rootpos;
	getFromPos(stream,IdxFile::kRootPointerPos,rootpos);
	// the first byte determines if root node is a leaf
	char byte;
	getFromPos(stream,rootpos,byte);
	switch (byte) {
	case IdxFile::LEAF:
	case IdxFile::OVERFLOW:
		root_ = new Leaf();
		Leaf *root = root_;
		binary_read(stream,rootpos + 1,root->slotuse);
		break;
	default:
		root_ = new InnerNode();
		InnerNode *root = root_;
		binary_read(stream,rootpos + 1,root->slotuse);
	}
}

template <typename KeyType, typename ValType, size_t BlockSize>
Node* BPTree<KeyType,ValType,BlockSize>::
		load_node_(std::fstream &stream, FilePos nodepos) {

}

template <typename KeyType, typename ValType, size_t BlockSize>
ValType BPTree<KeyType,ValType,BlockSize>::
		find(const KeyType &key) const throw(BPTree<KeyType,ValType,BlockSize>::NodeNotFound) {

	// TODO throw NodeNotFound when not found
	Node *p = root_;
	while (!p->isLeaf()) {
		InnerNode *inner_node = dynamic_cast<InnerNode*>(p);
		size_t next_child_index = find_lower_(p, key);
		p = inner_node->children[next_child_index];
	}
	Leaf *leaf_node = dynamic_cast<Leaf*>(p);

	// Search in inner node, find correct children and continue the recursion
	size_t data_index = find_lower_(p, key);
	return leaf_node->data[data_index];
}

template <typename KeyType, typename ValType, size_t BlockSize>
template <typename NodeWithKeys>
size_t BPTree<KeyType,ValType,BlockSize>::
		find_lower_(NodeWithKeys *p, const KeyType &key) {

	// Linear search inside a node to find the place that is just smaller than or equal to key
	// If kOrder is too large, use binary search instead
	size_t i;
	for (i = 0; i != p->slotuse; ++i)
		if (p->keys[i] >= key)
			break;
	return i;
}

template <typename KeyType, typename ValType, size_t BlockSize>
void BPTree<KeyType,ValType,BlockSize>::
		insert(const KeyType &key, const ValType &value) {

	std::stack<Node*> parent_trace;
	std::stack<Node*> index_trace;
	Node *p = root_;
	while (!p->isLeaf()) {
		InnerNode *inner_node = dynamic_cast<InnerNode*>(p);
		size_t next_child_index = find_lower_(p, key);
		index_trace.push(next_child_index);
		parent_trace.push(p);
		p = inner_node->children[next_child_index];
	}
	Leaf *leaf_node = dynamic_cast<Leaf*>(p);

	if (!insert_in_leaf_(p, key, value)) {
		return;
	} else {
		// split current node
		bool split_parent = true;
		do {
			Node *parent = parent_trace.pop();
			size_t prev_index = index_trace.pop();
			if (p->isLeaf()) { // p == leaf_node here
				size_t newval_pos = find_lower_(leaf_node, key);
				size_t mid_pos = (leaf_node->slotuse - 1)/2;
				KeyType mid_key = leaf_node->keys[mid_pos];
				ValType mid_val = leaf_node->data[mid_pos];
				Leaf *new_leaf = new Leaf();
				// copy the larger part to the new leaf
				if (newval_pos <= mid_pos) {
					// TODO new data should be placed in old leaf

				} else {
					// TODO new data should be placed in new leaf
				}

			} else { // p is inner node
				// TODO do the same above
			}
		} while (split_parent);
	}
}

#endif // BPTREE_HPP
