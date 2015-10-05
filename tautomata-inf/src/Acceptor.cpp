// $Id: Acceptor.cpp 377 2011-01-09 04:02:16Z babic $

#include "Acceptor.h"
#include "Term.h"
#include <algorithm>

using namespace term;

class Equal {
    typedef std::map<const Term*, const Term*> TermMapTy;
    typedef std::map<const Term*, unsigned> SubtermSizeMapTy;

    TermMapTy equals;
    unsigned matched;

public:
    Equal() : matched(0) {}

    bool operator()(const Term* t, const Term* u) {
        TermMapTy::const_iterator I = equals.find(t);
        if (I != equals.end()) {
            return I->second == u;
        }

        if (t->getId() != u->getId()) {
            return false;
        }

        assert(t->getNumOps() == u->getNumOps() &&
            "IDs match, but # of operands don't?");

        for (Term::const_ops_iterator TI = t->ops_begin(), TE =
                t->ops_end(), UI = u->ops_begin(); TI != TE; ++TI, ++UI)
        {
            const Term* tSubt = *TI;
            const Term* uSubt = *UI;
            if (!(*this)(tSubt, uSubt)) {
                return false;
            }
        }

        // Ok, all subterms match
        matched++;
        equals[t] = u;
        return true;
    }

    unsigned getNumMatched() const { return matched; }
};

class Collect {
    typedef std::set<const term::Term*> TermSetTy;

    const unsigned soughtId;
    const unsigned matchingIdx;
    TermSetTy&     matches;
    std::set<const Term*> visited;

public:

    Collect(unsigned id, unsigned idx, TermSetTy& m) : soughtId(id),
    matchingIdx(idx), matches(m) {}

    void visit(const Term* t) {
        assert(visited.find(t) == visited.end() &&
                "Revisiting a predecessor?");

        visited.insert(t);

        for (Term::const_use_iterator I = t->use_begin(), E =
                t->use_end(); I != E; ++I) {
            const Term* u = *I;
            if (u->isState()) { // State
                if (visited.find(u) == visited.end()) { 
                    visit(u);
                }
            } else if (u->getId() == soughtId) { // Term
                assert(matchingIdx < u->getNumOps() &&
                        "Index out of bounds.");
                if ((*u)[matchingIdx] == t) {
                    // Match
                    matches.insert(u);
                }
            }
        }
    }
};

void Acceptor::visit(Term* t) {
    assert(!t->isVisited() && "Node already visited?");
    t->markVisited();
    visited.push_back(t);

    {   // Try to find an exact match first

        Equal eq;
        TermMap::const_iterator_pair CIP = tm.equal_range(t->getHash());

        for (; CIP.first != CIP.second; ++CIP.first) {
            if (eq(t, CIP.first->second)) {
                termsMatched += eq.getNumMatched();
                match[t].insert(CIP.first->second);
                assert(match[t].size() == 1 && 
                    "Unexpected multiple matches?");
            }
        }

        if (!match[t].empty()) {
            if (t->isFinal()) {
                finalStatesReached++;
                distributionOfFinalStates[t->termDepth()]++;
            }

            // In order to match exactly, there should be no states
            // reachable from 't', thus there should exist only one
            // term in tm matching 't' exactly.
            assert(match[t].size() == 1 && "CSE failure?");
            return;
        }
    }

    // No exact match 

    // Visit all successors
    for (Term::const_ops_iterator I = t->ops_begin(), E = t->ops_end();
            I != E; ++I) {
        Term* x = *I;
        assert(x->getNumUses() > 0 && "Suc has no predecessors?");
        if (!x->isVisited()) {
            visit(x);
        }

        if (match[x].empty()) {
            return;
        }
    }
    
    // Since there's no exact match, find an intersection of all
    // predecessors of each successor and check if any of them matches
    // this term.
    TermSetTy& candidates = match[t];
    assert(candidates.empty() && "Corrupt initial state?");
    TermSetTy tmp;
    unsigned idx = 0;

    if (!t->isLeaf()) {
        //std::cerr << "-(" << t->getNumOps() << ')';

        for (Term::const_ops_iterator I = t->ops_begin(), E =
                t->ops_end(); I != E; ++I) {
        //std::cerr << "*";
            const Term* x = *I;
            TermSetTy& xmatch = match[x];
            assert(tmp.empty() && "Empty tmp set expected.");
            assert(!xmatch.empty() && "Successor has no matches?");
            assert(x->getNumUses() > 0 && "Suc has no predecessors?");

            // Collect all predecessors that have:
            // * the same ID as the 't' term
            // * have subterm 'x' in the idx-th position
            // and compute their intersection
            for (TermSetTy::const_iterator XI = xmatch.begin(), XE =
                    xmatch.end(); XI != XE; ++XI) {
                Collect collect(t->getId(), idx, tmp);
                (void)collect.visit(*XI);
            }

#ifndef NDEBUG
            for (TermSetTy::const_iterator TI = tmp.begin(), TE =
                    tmp.end(); TI != TE; ++TI) {
                const Term& tt = **TI;
                assert(tt.getId() == t->getId() && "Id mismatch.");
                /*
                if (tt[idx] != x && !tt[idx]->isState()) {
                    x->print(std::cerr);
                    std::cerr << std::endl << "_____ ( " << idx << " ) _______" <<
                        std::endl;
                    const_cast<Term&>(tt).print(std::cerr);
                    std::cerr << std::endl << "_______________" <<
                        std::endl;
                }// */
                assert((tt[idx]->getId() == x->getId() || 
                    tt[idx]->isState()) && 
                        "Unexpected node type at idx.");
                assert(tt.getNumOps() == t->getNumOps() &&
                        "Invalid number of operands.");
            }
#endif

            if (I == t->ops_begin()) {
                assert(candidates.empty() && "Unexpected init state.");
                tmp.swap(candidates);
            } else {
                TermSetTy tmp2;
                std::set_intersection(tmp.begin(), tmp.end(),
                        candidates.begin(), candidates.end(),
                        utils::asso_inserter<TermSetTy>(tmp2));
                tmp2.swap(candidates);
            }

            if (candidates.empty()) {
        //std::cerr << "+";
                return;
            }

            ++idx;
            tmp.clear();
        }

        assert(!candidates.empty() && "Unexpected set computed.");


        termsMatched++; // 't' matched

        if (t->isFinal()) {
            /*
            t->print(std::cout);
                std::cout << '(' << t->getNumOps() << ")<<<<" << std::endl;
                (*candidates.begin())->print(std::cout);
                std::cout << '(' << (*candidates.begin())->getNumOps()
                    << ")<<<<" << std::endl;
                // */
            //This is an expensive assertion. It seems to be passing.
            //assert(t->checkDepth() && "Invalid depth.");
            finalStatesReached++;
            distributionOfFinalStates[t->termDepth()]++;
        }

    } else {
        // Leaves should have been matched exactly.
        return;
    }
}

Acceptor::~Acceptor() {
    for (TermVecTy::iterator I = visited.begin(), E = visited.end(); I
            != E; ++I) {
        (*I)->clearVisited();
    }
}

std::ostream& Acceptor::stats(std::ostream& os) const {
    os << "S Matched terms:    " << termsMatched << std::endl;
    os << "S Final states hit: " << finalStatesReached << std::endl;
    os << "S ------------------------------------------" << std::endl;
    for (U2UMapTy::const_iterator I = distributionOfFinalStates.begin(),
            E = distributionOfFinalStates.end(); I != E; ++I) {
        os << "S Final matched with depth " << I->first << ": " <<
            I->second << std::endl;
    }
    os << "S ------------------------------------------" << std::endl;
    return os;
}
