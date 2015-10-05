// $Id: Parser.cpp 376 2011-01-09 04:00:47Z babic $

#include "Parser.h"
#include "Term.h"

using namespace parser;

void Parser::eatSpace(std::istream& is) {
    while (!is.eof() && (is.peek() == ' ' || is.peek() == '\t')) {
        (void)is.get();
    }
}

void Parser::perror(std::ostream& err, unsigned lin, const char* msg)
    const {
    err << '@';
    if (!fname.empty()) {
        err << fname << ':';
    }
    err << lin << " Parse error: " << msg << std::endl;
    exit(EXIT_FAILURE);
}

void Parser::pwarn(std::ostream& err, unsigned lin, const char* msg)
    const {
    err << '@';
    if (!fname.empty()) {
        err << fname << ':';
    }
    err << lin << " Parse warning: " << msg << std::endl;
}

void Parser::getRoots(TermMap& mp, term::Term::term_vector& roots) {
    for (TermMap::const_iterator I = mp.begin(), E = mp.end(); I != E;
            ++I) {
        if (I->second->isFinal()) {
            roots.push_back(I->second);
        }
    }
}

void Parser::deleteNodes(TermMap& mp) {
    for (TermMap::const_iterator I = mp.begin(), E = mp.end(); I != E;
            ++I) {
        delete I->second;
    }
    mp.clear();
}
