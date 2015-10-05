// $Id: TermVisitor.cpp 351 2010-12-29 08:55:24Z babic $

#include "TermVisitor.h"

using namespace term;

void Visitor::visitPre(Term* t) {
    for (Term::use_iterator I = t->use_begin(), E = t->use_end(); I !=
            E; ++I) {
        Term* t = *I;
        assert(t && "Unexpected NULL ptr.");
        if (!t->isVisited()) {
            visit(t);
        }
    }
}

void Visitor::visitPost(Term* t) {
    for (Term::ops_iterator I = t->ops_begin(), E = t->ops_end(); I !=
            E; ++I) {
        Term* t = *I;
        assert(t && "Unexpected NULL ptr.");
        if (!t->isVisited()) {
            visit(t);
        }
    }
}

void Visitor::unmark() {
    for (Term::term_vector::iterator I = visited.begin(), E =
            visited.end(); I != E; ++I) {
        Term* t = *I;
        assert(t && "Unexpected NULL ptr.");
        t->clearVisited();
    }
}

void POVisitor::visit(Term* t) {
    assert(!t->isVisited() && "Visiting an already visited node?");
    t->markVisited();
    visited.push_back(t);
    visitPost(t);
    visitNode(t);
}

void DFSVisitor::visit(Term* t) {
    assert(!t->isVisited() && "Visiting an already visited node?");
    t->markVisited();
    visited.push_back(t);
    visitNode(t);
    visitPost(t);
}

void BFSVisitor::visit(Term::term_vector& tv) {
    Term::term_vector succ;
    for (Term::term_vector::iterator I = tv.begin(), E = tv.end(); I !=
            E; ++I) {
        Term* t = *I;
        if (!t->isVisited()) {
            t->markVisited();
            visited.push_back(t);
            visitNode(t);
            for (Term::ops_iterator SI = t->ops_begin(), SE =
                    t->ops_end(); SI != SE; ++SI) {
                Term* st = *SI;
                if (!st->isVisited()) {
                    succ.push_back(st);
                }
            }
        }
    }

    if (!succ.empty()) {
        visit(succ);
    }
}

void BFSVisitor::visit(Term* t) {
    Term::term_vector succ;
    assert(!t->isVisited() && "Visiting an already visited node?");
    t->markVisited();
    visited.push_back(t);
    for (Term::ops_iterator SI = t->ops_begin(), SE = t->ops_end(); SI
            != SE; ++SI) {
        Term* st = *SI;
        if (!st->isVisited()) {
            succ.push_back(st);
        }
    }
    visit(succ);
}

void FullyDefined::visitPost(Term* t) {
    if (!t) {
        defined = false;
        return;
    }

    for (Term::ops_iterator I = t->ops_begin(), E = t->ops_end(); I !=
            E; ++I) {
        Term* t = *I;
        if (!t) {
            defined = false;
            return;
        } else if (!t->isVisited()) {
            visit(t);
        }
    }
}
