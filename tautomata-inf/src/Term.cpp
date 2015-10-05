// $Id: Term.cpp 376 2011-01-09 04:00:47Z babic $

#include "Term.h"
#include "Utilities.h"
#include <cassert>
#include <iostream>

using namespace term;
    
std::size_t TermHash::operator()(const Term* t) const {
    return t->getHash(); 
}

bool Term::equal(const Term& t) const {
    if (t.getHash() != getHash() || t.getId() != getId() ||
            t.getNumOps() != getNumOps()) {
        return false;
    }
    for (const_ops_iterator I = ops_begin(), E = ops_end(), TI =
            t.ops_begin(); I != E; ++I, ++TI) {
        if (!(*I)->equal(**TI)) {
            return false;
        }
    }
    return true;
}

unsigned Term::computeDepth() const {
    if (isLeaf()) {
        return 0;
    } else {
        unsigned depth = 0;
        for (term_vector::const_iterator I = ops_begin(), E = ops_end();
                I != E; ++I) {
            const Term* t = *I;
            depth = utils::max<unsigned>(t->computeDepth(), depth);
        }
        return depth + 1;
    }
}

Term::Term(const std::string* nm, const term_vector& ops, unsigned id,
        bool state) : name(nm), id(id), depth(0), visited(false),
    state(state) {

    operands.reserve(ops.size());
    node_hash = (unsigned)utils::inthash((int)id);
    int pos = 1;

    bool hasOperands = false;
    for (term_vector::const_iterator I = ops.begin(), E = ops.end(); I
            != E; ++I) {
        Term* t = *I;
        hasOperands = true;
        operands.push_back(t);
        if (t) {
            node_hash ^= (t->getHash() * pos);
            depth = utils::max<unsigned>(depth, t->termDepth());
            t->addUse(this);
        }
        pos++;
    }

    if (hasOperands) {
        depth++;
        node_hash *= depth;
    }
}

void Term::addOp(Term* t) {
    operands.push_back(t);
    t->addUse(this);
}

bool Term::substOp(Term* del, Term* rep) {
    bool someSubstituted = false;
    for (ops_iterator I = ops_begin(), E = ops_end(); I != E; ++I) {
        if (*I == del) {
            *I = rep;
            someSubstituted = true;
        }
    }

    if (someSubstituted) {
        rep->addUse(this);
    }
    return someSubstituted;
}

void Term::recomputeHashAndDepth() {
    node_hash = (unsigned)utils::inthash((int)id);
    depth = 0;
    int pos = 1;
    bool hasOperands = false;

    for (term_vector::const_iterator I = ops_begin(), E = ops_end(); I
            != E; ++I) {
        Term* t = *I;
        hasOperands = true;
        assert(t && "Unexpected NULL ptr.");
        node_hash ^= (t->getHash() * pos++);
        depth = utils::max<unsigned>(depth, t->termDepth());
    }

    if (hasOperands) {
        depth++;
        node_hash *= depth;
    }
}

void Term::recomputeKLevHash() {
    if (termDepth() == 0) {
        klevel_hash = getHash();
    } else {
        int pos = 1;
        klevel_hash = (unsigned)utils::inthash((int)id);
        for (term_vector::const_iterator I = ops_begin(), E = ops_end(); I
            != E; ++I) {

            Term* t = *I;
            assert(t && "Unexpected NULL ptr.");
            klevel_hash ^= (t->getKLevHash() * pos++);
        }
    }
}

bool Term::operator==(const Term& t) const {
    const bool res = (&t == this);
    // This is an expensive assertion
    //assert(res == equal(t) && "Term not a singleton?");
    return res;
}

bool Term::operator!=(const Term& t) const {
    const bool res = (&t != this);
    // This is an expensive assertion
    //assert(res == !equal(t) && "Term not a singleton?");
    return res;
}

bool Term::klevEqual(const Term& t, int k) const {
    assert(k >= 0 && "Invalid number of levels.");

    if (t.getId() != getId() || t.getNumOps() != getNumOps()) {
        return false;
    }

    if (k == 0) {
        return true;
    }

    k--;

    for (const_ops_iterator I = ops_begin(), E = ops_end(), TI =
            t.ops_begin(); I != E; ++I, ++TI) {

        if (!(*I)->klevEqual(**TI, k)) {
            return false;
        }
    }
    return true;
}

bool Term::check() const {
    for (const_ops_iterator I = ops_begin(), E = ops_end(); I!=E; ++I) {
        if (*I == 0) {
            return false;
        }
        if ((*I)->termDepth() > termDepth()) {
            return false;
        }
    }
    return true;
}

bool Term::checkDepth() const {
    const unsigned dpth = computeDepth();
    if (dpth != termDepth()) {
        std::cerr << "Computed depth: " << dpth << 
            ", constructor depth " << termDepth() << std::endl;
        return false;
    }
    return true;
}

void Term::doPrint(std::ostream& os, use_list& visited) {
    if (isVisited()) {
        os << this;
        return;
    }

    markVisited();
    visited.insert(this);

    os << getId();
    if (name) {
        os << ':' << *name << '(' << this << ')';
    }
    if (!isLeaf()) {
        os << '(';
        for (ops_iterator I = ops_begin(), E = ops_end(); I!=E; ++I) {
            Term* t = *I;
            t->doPrint(os, visited);
            os << ' ';
        }
        os << ')';
    }
}

void Term::print(std::ostream& os) {
    use_list visited;
    doPrint(os, visited);
    for (const_use_iterator I = visited.begin(), E = visited.end(); I !=
            E; ++I) {
        (*I)->clearVisited();
    }
}

void Term::print(std::ostream& os) const {
    os << getId();
    if (name) {
        os << ':' << *name;
    }
    if (!isLeaf()) {
        os << '(';
        for (const_ops_iterator I = ops_begin(), E = ops_end(); I!=E;
                ++I) {
            const Term* t = *I;
            t->print(os);
            os << ' ';
        }
        os << ')';
    }
}

bool TermSorter::operator()(const Term* t, const Term* u) const {
    assert(t && u && "Unexpected NULL ptrs.");
    const unsigned tid = t->getId();
    const unsigned uid = u->getId();
    const unsigned tdpth = t->termDepth();
    const unsigned udpth = u->termDepth();
    return tid < uid || 
        (tid == uid && tdpth < udpth) ||
        (tid == uid && tdpth == udpth && t->getHash() < u->getHash());
}
