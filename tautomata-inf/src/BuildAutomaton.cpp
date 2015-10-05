// $Id: BuildAutomaton.cpp 376 2011-01-09 04:00:47Z babic $

#include "BuildAutomaton.h"

using namespace term;
using namespace enode;

void BuildAutomaton::visitNode(Term* t) {
    assert(enodes.find(t) != enodes.end() && "Term not found.");
    ENode& representative = *enodes[t]->getRepresentative();
    Term* rep = representative.getVal();
    const unsigned classSize = representative.getEquivalenceClassSize();
    assert(classSize >= 1 && "Invalid eq class size.");

    // Create new states only for the representative when the
    // equivalence class is of non-trivial (> 1) size
    if (classSize == 1 || rep != t) {
        return;
    }

    Term::term_vector ops;

#ifndef NDEBUG
    bool mustHaveUses = false;
#endif

    for (ENode::iterator I = representative.begin(), E =
            representative.end(); I != E; ++I) {
        Term* member = (*I)->getVal();
#ifndef NDEBUG
        mustHaveUses |= !member->isFinal();
#endif
        assert(member && "Unexpected NULL ptr.");
        // This check could fail because of graph modifications done
        // below.
        //assert(member->check() && "Invalid subterm.");
        ops.push_back(member);
    }

    assert(!ops.empty() && "Invalid equivalence class.");
    Term* state = new Term(0, ops, ++counter, true);
    state->markVisited();
    visited.push_back(state);
    automaton.insert(std::make_pair(state->getHash(), state));

    for (ENode::iterator I = representative.begin(), E =
            representative.end(); I != E; ++I) {
        Term* member = (*I)->getVal();
        for (Term::use_iterator MI = member->use_begin(), ME =
                member->use_end(); MI != ME; ++MI) {
            Term* u = *MI;
            if (u == state) {
                continue;
            }
            (void)u->substOp(member, state);
        }
        member->clearUse();
        member->addUse(state);
        assert(member->getNumUses() == 1 && "Invalid use update.");
    }
    assert(!state->isFinal() == mustHaveUses &&
            "State improperly spliced.");
}
