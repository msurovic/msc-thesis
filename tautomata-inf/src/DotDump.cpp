// $Id: DotDump.cpp 345 2010-12-29 02:11:21Z babic $

#include "DotDump.h"
#include "Parser.h"

using namespace term;

void DotDump::printHeader() {
    assert(os && "Unexpected NULL ptr.");
    (*os) << "digraph {" << std::endl;
}

void DotDump::printFooter() {
    assert(os && "Unexpected NULL ptr.");
    (*os) << "}" << std::endl;
}

void DotDump::visitNode(Term* t) {
    assert(os && "Unexpected NULL ptr.");
    (*os) << '\t' << t->getId() << " [label=\"" << 
        t->getId() << ':';
    if (t->getName()) {
        (*os) << *t->getName();
    }
    (*os) << ':' << t->getKLevHash() << "\"];" << std::endl;

    if (t->getNumOps() > 0) {
        (*os) << '\t' << t->getId() << " -> {";
        for (Term::ops_iterator I = t->ops_begin(), E = t->ops_end(); I
                != E;) {
            (*os) << (*I)->getId();
            ++I;
            if (I != E) {
                (*os) << "; ";
            }
        }
        (*os) << "};\n";
    }
}

void DotDump::print(std::ostream& s) {
    os = &s;
    printHeader();
    for (Term::term_vector::const_iterator I = roots.begin(), E =
            roots.end(); I != E; ++I) {
        visit(*I);
    }
    printFooter();
    os = 0;
}
