// $Id: CSE.h 343 2010-12-28 04:46:40Z babic $

#ifndef COMMON_SUBEXPRESSION_ELIMINATION_H
#define COMMON_SUBEXPRESSION_ELIMINATION_H

#include "TermVisitor.h"
#include "Utilities.h"
#include <stdexcept>

namespace term {

class MissingEdge : public std::exception {
    unsigned id;
    unsigned idx;
public:
    MissingEdge(unsigned id, unsigned idx) : id(id), idx(idx) {}
    unsigned getId() const { return id; }
    unsigned getIdx() const { return idx; }
};

class CSE : public POVisitor {
public:
    typedef utils::Multimap<unsigned, term::Term*> TermMap;
private:
    TermMap& tm;
    utils::Map<term::Term*, term::Term*> translation;
public:
    CSE(TermMap& tmap) : tm(tmap) {}
    void visitNode(Term*);
};

} // End of the term namespace

#endif // COMMON_SUBEXPRESSION_ELIMINATION_H
