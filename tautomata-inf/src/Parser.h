// $Id: Parser.h 376 2011-01-09 04:00:47Z babic $

#ifndef PARSER_H
#define PARSER_H

#include "Utilities.h"
#include <iostream>
#include <vector>
#include <string>

// Forward declaration
namespace term {
class Term;
} // End of the term namespace

namespace parser {

class Parser {
    std::string fname;
protected:
    void eatLine(std::istream& is) {
        while (!is.eof() && is.get() != '\n') { }
    }
    void eatSpace(std::istream&);
public:
    typedef utils::Multimap<unsigned, term::Term*> TermMap;
    typedef std::vector<term::Term*> term_vector;

    virtual void parse(std::istream&, TermMap&) = 0;
    virtual std::ostream& stats(std::ostream&) const = 0;
    virtual void perror(std::ostream&, unsigned line, const char* msg)
        const;
    virtual void pwarn(std::ostream&, unsigned line, const char* msg)
        const;
    virtual void setFilename(const std::string& s) { fname = s; }
    virtual const std::string& getFilename() const { return fname; }
    virtual void computeStats(TermMap&) = 0;
    //virtual unsigned getNumParsedNodes() const = 0;

    static void getRoots(TermMap&, term_vector&);
    static void deleteNodes(TermMap&);

    // Debug
    virtual void printNameTable() const {}
};

} // End of the parser namespace

#endif // PARSER_H
