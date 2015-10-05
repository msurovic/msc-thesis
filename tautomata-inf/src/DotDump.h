// $Id: DotDump.h 345 2010-12-29 02:11:21Z babic $

#ifndef DOTDUMP_H
#define DOTDUMP_H

#include "TermVisitor.h"
#include <iostream>

// Forward declaration
namespace parser {
class Parser;
} // End of the parser namespace

namespace term {

class DotDump : public POVisitor {
    const Term::term_vector& roots;
    std::ostream* os;

    void printHeader();
    void printFooter();
public:
    DotDump(const Term::term_vector& r) : roots(r), os(0) {}
    virtual void visitNode(Term*);
    void print(std::ostream&);
};

} // End of the term namespace

#endif // DOTDUMP_H
