// $Id: BuildAutomaton.h 371 2011-01-06 20:05:12Z babic $

#ifndef BUILD_AUTOMATON_H
#define BUILD_AUTOMATON_H

#include "TermVisitor.h"
#include "Utilities.h"
#include "ENode.h"

namespace term {

class BuildAutomaton : public POVisitor {
public:
    typedef utils::Multimap<unsigned, term::Term*> TermMap;
    typedef enode::ENode<Term> ENode;
    typedef utils::Map<Term*, ENode*, TermHash> Term2ENodeMapTy;
private:
    unsigned counter;
    Term2ENodeMapTy& enodes;
    TermMap& automaton;
public:
    BuildAutomaton(unsigned N, Term2ENodeMapTy& e, TermMap& a) : 
        counter(N), enodes(e), automaton(a) {}
    void visitNode(Term*);
    unsigned getCounter() const { return counter; }
};

} // End of the term namespace

#endif // BUILD_AUTOMATON_H
