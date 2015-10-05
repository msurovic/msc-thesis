// $Id: Acceptor.h 371 2011-01-06 20:05:12Z babic $

#ifndef TAUTOMATA_ACCEPTOR_H
#define TAUTOMATA_ACCEPTOR_H

#include "Utilities.h"
#include <iostream>
#include <vector>
#include <set>
#include <map>

namespace term {

class Term;

class Acceptor {
public:
    typedef utils::Multimap<unsigned, term::Term*> TermMap;
    typedef std::vector<Term*> TermVecTy;
    typedef std::set<const term::Term*> TermSetTy;
    typedef std::map<const term::Term*, TermSetTy> MatchSetTy;
    typedef std::map<unsigned, unsigned> U2UMapTy;
private:
    const TermMap& tm;

    TermVecTy   visited;
    MatchSetTy  match;

    // Statistics
    unsigned termsMatched;
    unsigned finalStatesReached;
    U2UMapTy distributionOfFinalStates; 

public:
    Acceptor(const TermMap& tm) : tm(tm), termsMatched(0),
    finalStatesReached(0){}
    ~Acceptor();
    void visit(Term*);
    std::ostream& stats(std::ostream&) const;
};

} // End of the term namespace

#endif // TAUTOMATA_ACCEPTOR_H
