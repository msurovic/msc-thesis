// $Id: CSE.cpp 353 2010-12-31 10:37:02Z babic $

#include "CSE.h"
#include <algorithm>
#include <iostream>

using namespace term;

void CSE::visitNode(Term* t) {
    t->recomputeHashAndDepth();
    TermMap::const_iterator_pair CIP = tm.equal_range(t->getHash());
    for (; CIP.first != CIP.second; ++CIP.first) {
        Term* ft = CIP.first->second;
        if (ft->getId() != t->getId() || ft->termDepth() !=
                t->termDepth()) {
            continue;
        }

        bool found = true;
        for (Term::ops_iterator I = t->ops_begin(), E = t->ops_end(),
                TI = ft->ops_begin(); I != E;
                ++I, ++TI) {
            Term* sub = *I;
            if (!sub) {
                throw(MissingEdge(t->getId(),
                            std::distance(t->ops_begin(), I)));
            }
            assert(translation.find(sub) != translation.end() &&
                    "Invalid PO visit order.");
            if (*translation[sub] != **TI) {
                found = false;
                break;
            }
        }
        if (!found) {
            continue;
        }
        translation[t] = ft;
        return;
    }

    Term::term_vector ops;
    for (Term::ops_iterator I = t->ops_begin(), E = t->ops_end(); I !=
            E; ++I) {
        Term* sub = *I;
        assert(sub && "Unexpected NULL ptr.");
        assert(translation.find(sub) != translation.end() &&
                "Invalid PO visit order.");
        Term* trans = translation[sub];
        assert(trans && "Unexpected NULL ptr.");
        assert(trans->getId() == sub->getId() && "ID mismatch.");
        assert(trans->getHash() == sub->getHash() && "Hash mismatch.");
        assert(trans->termDepth() == sub->termDepth() && 
                "Depth mismatch.");
        ops.push_back(trans);
    }
    Term* nt = new Term(t->getName(), ops, t->getId());
    translation[t] = nt;
    assert(t->getHash() == nt->getHash() && "Hash mismatch?");
    tm.insert(std::make_pair(nt->getHash(), nt));
}
