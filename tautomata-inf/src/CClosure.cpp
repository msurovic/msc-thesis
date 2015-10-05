// $Id: CClosure.cpp 444 2011-01-16 07:40:50Z babic $

#include "CClosure.h"
#include "Parser.h"
#include "BuildAutomaton.h"
#include <map>
#include <algorithm>

using namespace term;

void Closure::checkin(Term* t) {
    assert(t->check() && "Invalid term.");
    if (checkedIn(t)) {
        return; // Already checked in
    }

    t2e[t] = new ENode(t);
    assert(t2e[t]->check() && "Invalid ENode.");

    for (Term::use_iterator I = t->use_begin(), E = t->use_end(); I !=
            E; ++I) {
        Term* use = *I;
        uselist[t].insert(use);
    }

    assert(t2e[t]->getEquivalenceClassSize() > 0 &&
            "Invalid equivalence class size.");
    assert(t2e[t]->check() && "Invalid ENode.");
    
    lookupTable.insert(std::make_pair(hash(t), t));
}

Closure::~Closure() {
    for (Term2ENodeMapTy::const_iterator I = t2e.begin(), E =
            t2e.end(); I != E; ++I) {
        ENode* e = I->second;
        delete e;
    }
    t2e.clear();
}

void Closure::merge(Term* t, Term* u) {
    wlist.push_back(std::make_pair(t, u));
}

void Closure::propagate() {
    Term::term_vector uses;

    while (!wlist.empty()) {
        assert(checkedIn(wlist.back().first) && 
               checkedIn(wlist.back().second) &&
               "Term(s) not properly checked in.");

        Term* tt = wlist.back().first;
        Term* ut = wlist.back().second;
        wlist.pop_back();

        if (tt == ut) {
            continue;
        }

        ENode* t = t2e[tt]->getRepresentative();
        ENode* u = t2e[ut]->getRepresentative();

        assert(t && u && "Unexpected NULL ptr(s).");
        assert(t->check() && u->check() && "Invalid ENodes.");
        assert(t->isClassRepresentative() &&
               u->isClassRepresentative() &&
               "Function is supposed to work on representatives.");
        assert(t->getEquivalenceClassSize() > 0 &&
               u->getEquivalenceClassSize() > 0 &&
               "Eq classes are supposed to be non-empty here.");

        if (*t != *u) { // Not in the same equivalence class
            if (*u <= *t) {
                ENode* tmp = t; t = u; u = tmp;
            }
            assert(*t <= *u && "Failed swap.");
            assert(t->check() && "Invalid ENode.");
            uses.clear();
            numMergedClasses++;

            {
                // Cleanup the lookup table --- remove all uses of t
                // from the lookup table, as at least one of their
                // operands will change (effectively)

                TermSetTy& tset = uselist[tt];

                for (TermSetTy::iterator I = tset.begin(), E =
                        tset.end(); I != E; ++I) {

                    Term* use = *I;
                    uses.push_back(use);
                    LookupTableTy::const_iterator_pair CIP =
                        lookupTable.equal_range(hash(use));

                    for (; CIP.first != CIP.second; ) {
                        Term* x = CIP.first->second;
                        if (use == x) {
                            lookupTable.erase(CIP.first++);
                        } else {
                            ++CIP.first;
                        }
                    }
                }

                uselist.erase(tt);
            }
            assert(t->check() && "Invalid ENode.");

            if (ut->getNumOps() == tt->getNumOps() && ut->getId() ==
                    tt->getId()) {
                Term* ux = 0;
                Term* tx = 0;
                for (Term::ops_iterator UI = ut->ops_begin(), TI =
                        tt->ops_begin(), UE = ut->ops_end(); UI != UE;
                        ++UI) {
                    ux = *UI;
                    tx = *TI;
                    assert(checkedIn(ux) && "Node not checked in?");
                    assert(checkedIn(tx) && "Node not checked in?");
                    if (*t2e[ux] != *t2e[tx]) {
                        ux = 0;
                        tx = 0;
                        break;
                    }
                }
                if (ux && tx) {
                    merge(ux, tx);
                }
            }// */

            *u += *t;

            assert(t->check() && "Invalid ENode.");

#ifndef NDEBUG
            const unsigned presortSize = uses.size();
#endif

            TermSetTy& uset = uselist[ut];
            // An attempt to make things more deterministic
            std::sort(uses.begin(), uses.end(), TermSorter());
            assert(uses.size() == presortSize && "Sorting error.");

            // Update the use list
            for (Term::term_vector::const_iterator UI = uses.begin(), UE
                    = uses.end(); UI != UE; ++UI) {
                Term* use = *UI;
                // It is necessary to recompute the hash, as the
                // representative of 't' has changed.
                const unsigned useHash = hash(use);
                uset.insert(use);
                
                LookupTableTy::const_iterator_pair CIP =
                    lookupTable.equal_range(useHash);

                for (; CIP.first != CIP.second; ++CIP.first) {
                    Term* x = CIP.first->second;
                    if (use == x || use->getId() != x->getId()) {
                        continue;
                    }
                    assert(use->getNumOps() == x->getNumOps() &&
                            "Mismatch in the number of operands.");

                    bool newCongruenceFound = true;
                    for (Term::const_ops_iterator I = use->ops_begin(),
                            E = use->ops_end(), XI = x->ops_begin(); I
                            != E; ++I, ++XI) {
                        Term* opUse = *I;
                        Term* opX = *XI;
                        assert(checkedIn(opUse) && checkedIn(opX) &&
                                "Operand(s) not checked in?");
                        if (*t2e[opUse] != *t2e[opX]) {
                            newCongruenceFound = false;
                            break;
                        }
                    }

                    if (newCongruenceFound) {
                        numCongruences++;
                        merge(use, x);
                    }
                }

                lookupTable.insert(std::make_pair(useHash, use));
            }
        } 

        assert(t->check() && "Invalid ENode.");
        assert(u->check() && "Invalid ENode.");
    }
}

void Closure::rebuildAutomaton(unsigned N, TermMap& a) {
    Term::term_vector roots;
    parser::Parser::getRoots(a, roots);

    BuildAutomaton ba(N, t2e, a);
    for (Term::term_vector::iterator I = roots.begin(), E = roots.end();
            I != E; ++I) {
        ba.visit(*I);
    }
    numStatesCreated = ba.getCounter() - N;
}

std::ostream& Closure::stats(std::ostream& os) const {
    typedef std::map<int, int> StatsTableTy;

    StatsTableTy depthDistribution;
    unsigned classes = 0;
    unsigned rootClasses = 0;

    os << "S Merged eq classes:" << numMergedClasses << std::endl;
    os << "S Congruences:      " << numCongruences << std::endl;

    for (Term2ENodeMapTy::const_iterator I = t2e.begin(), E =
            t2e.end(); I != E; ++I) {
        const ENode& e = *I->second;
        if (e.isClassRepresentative()) {
            classes++;
            if (e.getVal()->isFinal()) {
                rootClasses++;
                const unsigned dpth = e.getVal()->termDepth();
                StatsTableTy::iterator SI = depthDistribution.find(dpth);
                if (SI == depthDistribution.end()) {
                    depthDistribution[dpth] = 1;
                } else {
                    SI->second++;
                }
            }
        }
    }
    os << "S Eq classes:       " << classes << std::endl;
    os << "S Eq root classes:  " << rootClasses << std::endl;
    os << "S States created:   " << numStatesCreated << std::endl;
    os << "S ------------------------------------------" << std::endl;
    for (StatsTableTy::const_iterator I = depthDistribution.begin(), E =
            depthDistribution.end(); I != E; ++I) {
        os << "S Eq rt class with depth " << I->first << ": " << I->second <<
            std::endl;
    }
    os << "S ------------------------------------------" << std::endl;

    return os;
}

