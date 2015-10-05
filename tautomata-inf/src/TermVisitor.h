// $Id: TermVisitor.h 351 2010-12-29 08:55:24Z babic $

#ifndef TERM_VISITOR_H
#define TERM_VISITOR_H

#include "Term.h"

namespace term {

class Visitor {
protected:
    Term::term_vector visited;

    virtual void visitPre(Term*);
    virtual void visitPost(Term*);
    virtual void unmark();

public:
    virtual ~Visitor() { unmark(); }
    virtual void visit(Term*) = 0;
    virtual void visitNode(Term*) = 0;
};

// Post-order (successors first) visitor
class POVisitor : public Visitor {
public:
    virtual void visit(Term*);
};

// DFS (in-order) visitor
class DFSVisitor : public Visitor {
public:
    virtual void visit(Term*);
};

// BFS visitor
class BFSVisitor : public Visitor {
public:
    virtual void visit(Term::term_vector&);
    virtual void visit(Term*);
};

class FullyDefined : public DFSVisitor {
    bool defined;
    void visitPost(Term*);
public:
    FullyDefined() : defined(true) {}
    void visitNode(Term*) {}
    bool isFullyDefined() const { return defined; }
};

} // End of the term namespace

#endif // TERM_VISITOR_H
