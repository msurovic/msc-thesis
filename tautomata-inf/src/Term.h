// $Id: Term.h 376 2011-01-09 04:00:47Z babic $

#ifndef TERM_H
#define TERM_H

#include "Counted.h"
#include "HashFunctions.h"
#include "Utilities.h"
#include <list>
#include <vector>
#include <string>
#include <iostream>

namespace term {

class Term;

struct TermHash {
    std::size_t operator()(const Term*) const;
};

class Term {
    // In general, using pointers as hashes in associative data
    // structures is a big no-no. However, at this point the term might
    // not be completely formed, meaning that there might be a lot of
    // hash colisions. Thus, there's no choice but to use pointers.
    typedef utils::Set<Term*>  use_list;
    typedef std::vector<Term*> subt_list; // Subterms

    const std::string* name;
    unsigned id;
    unsigned depth;
    // Hash of the node itself
    unsigned node_hash;
    // Hash of k-level subtree 
    unsigned klevel_hash;
    use_list  uses;
    subt_list operands;
    bool visited;
    bool state;

    bool equal(const Term&) const;
    unsigned computeDepth() const;
    void doPrint(std::ostream&, use_list&);
public:

    typedef std::vector<Term*>  term_vector;
    typedef use_list::iterator  use_iterator;
    typedef subt_list::iterator ops_iterator;
    typedef use_list::const_iterator const_use_iterator;
    typedef subt_list::const_iterator const_ops_iterator;

    Term(unsigned id) : name(0), id(id), depth(0),
    node_hash((unsigned)utils::inthash((int)id)),
    klevel_hash(node_hash), visited(false) {}

    Term(const std::string* nm, const term_vector& ops, unsigned id,
            bool state = false);

    use_iterator use_begin()    { return uses.begin(); }
    use_iterator use_end()      { return uses.end(); }
    ops_iterator ops_begin()   { return operands.begin(); }
    ops_iterator ops_end()     { return operands.end(); }

    const_use_iterator use_begin() const    { return uses.begin(); }
    const_use_iterator use_end() const      { return uses.end(); }
    const_ops_iterator ops_begin() const   { return operands.begin(); }
    const_ops_iterator ops_end() const     { return operands.end(); }

    void addUse(Term* p) { uses.insert(p); }
    void clearUse() { uses.clear(); }
    bool substOp(Term* del, Term* rep);
    // Adding additional operands invalidates hashes and depth
    void addOp(Term*);
    unsigned getNumUses() const { return uses.size(); }
    unsigned getNumOps() const { return operands.size(); }
    unsigned getHash() const { return node_hash; }
    unsigned getKLevHash() const { return klevel_hash; }
    unsigned termDepth() const { return depth; }
    unsigned getId() const { return id; }
    void markVisited() { visited = true; }
    void clearVisited() { visited = false; }
    bool isVisited() const { return visited; }
    bool isFinal() const { return getNumUses() == 0; }
    bool isLeaf() const { return getNumOps() == 0; }
    bool isState() const { return state; }
    const std::string* getName() const { return name; }
    void recomputeHashAndDepth();
    // Note: Depends on depth being correct.
    void recomputeKLevHash();
    void clearKLevHash() { klevel_hash = 0; }

    bool operator==(const Term&) const;
    bool operator!=(const Term&) const;
    // Compares k-roots, i.e., the tree rooted at 'this' up to k levels
    // deep
    bool klevEqual(const Term&, int k) const;

    const Term* operator[](unsigned i) const {
        assert(i < getNumOps() && "Index out of bounds.");
        return operands[i];
    }
    Term*& operator[](unsigned i) {
        assert(i < getNumOps() && "Index out of bounds.");
        return operands[i];
    }

    // Debug
    bool check() const;
    bool checkDepth() const;
    void print(std::ostream&);
    // For acyclic graphs only
    void print(std::ostream&) const;
};

struct TermSorter {
public:
    bool operator()(const Term*, const Term*) const;
};

} // End of the term namespace

#endif // TERM_H
