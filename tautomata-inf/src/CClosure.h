// $Id: CClosure.h 371 2011-01-06 20:05:12Z babic $

#ifndef CONGRUENCE_CLOSURE_H
#define CONGRUENCE_CLOSURE_H

#include "ENode.h"
#include "Term.h"
#include "Utilities.h"
#include <vector>
#include <iostream>

namespace term {

class Closure {
    typedef enode::ENode<Term> ENode;
    typedef utils::Map<Term*, ENode*, TermHash> Term2ENodeMapTy;
    typedef utils::Multimap<unsigned, Term*> LookupTableTy;
    typedef utils::Set<Term*, TermHash> TermSetTy;
    typedef utils::Map<Term*, TermSetTy, TermHash> UseListTy;
    typedef std::vector<std::pair<Term*,Term*> > WorkListTy;

    Term2ENodeMapTy t2e;
    WorkListTy wlist;
    LookupTableTy lookupTable;
    UseListTy uselist;

    // Statistics
    unsigned numMergedClasses;
    unsigned numCongruences;
    unsigned numStatesCreated;

    bool checkedIn(Term* t) const { return t2e.find(t) != t2e.end(); }

    template <typename Iter>
    unsigned hash(Iter I, Iter E, unsigned id) {
        unsigned h = (unsigned)utils::inthash((int)id);
        int pos = 1;
        for (; I != E; ++I) {
            Term* t = *I;
            assert(t && "Unexpected NULL ptr.");
            if (checkedIn(t)) {
                h ^= (unsigned)utils::inthash(
                (int)t2e[t]->getRepresentative()->getVal()->getId())
                * pos++;
            } else {
                // Operands might not be checked in when checkin() is
                // called
                h ^= (unsigned)utils::inthash((int)t->getId()) * pos++;
            }
        }
        return h;
    }

    unsigned hash(Term* t) {
        return hash<Term::const_ops_iterator>(t->ops_begin(),
                t->ops_end(), t->getId());
    }

public:
    typedef utils::Multimap<unsigned, term::Term*> TermMap;

    Closure() : numMergedClasses(0), numCongruences(0) {}
    ~Closure();

    void checkin(Term*);
    void merge(Term*, Term*);
    void propagate();
    void rebuildAutomaton(unsigned, TermMap&);
    std::ostream& stats(std::ostream&) const;
};

} // End of the term namespace

#endif // CONGRUENCE_CLOSURE_H
