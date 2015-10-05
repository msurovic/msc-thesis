// $Id: DependencyParser.h 376 2011-01-09 04:00:47Z babic $

#ifndef DEPENDENCY_PARSER_H
#define DEPENDENCY_PARSER_H

#include "Parser.h"
#include "Term.h"
#include "Utilities.h"
#include <boost/bimap/bimap.hpp>
#include <map>
#include <vector>
#include <string>

namespace parser {

class DepParser : public Parser {
    typedef std::pair<unsigned, unsigned> NodeSignatureTy;
    typedef utils::Map<int, NodeSignatureTy> DeclTableTy;
    typedef std::vector<term::Term*> TermTableTy;
    typedef std::map<int, int> StatsTableTy;

    // It would be nice to replace this with unordered_map, but in the
    // std::tr1::hash<string> specialization is missing in some older
    // GCC libraries. In that case, the pointer of string is used for
    // hashing, resulting in a difficult to find error. Thus, it's
    // better to use std::map and play it safe. Alternatively, it would
    // be possible to explicitly provide a hash functor.
    typedef boost::bimaps::bimap<std::string, unsigned> NameTableTy;
    typedef std::map<unsigned, unsigned> BmarkSpecificMapTy;

    TermTableTy partialDefs;  // Cleared after every parse call
    BmarkSpecificMapTy local; // Cleared after every parse call

    NameTableTy& names;       // Reused for parsing multiple files
    DeclTableTy& signatures;  // Reused for parsing multiple files

    int nodesDeclared;
    unsigned line;
    unsigned nodesFound;
    unsigned edgesFound;
    unsigned rootsBeforeCSE;
    unsigned nodesAfterCSE;
    unsigned rootsAfterCSE;
    unsigned incompletelySpecifiedTerms;
    StatsTableTy depthDistribution;

    bool checkNodeId(unsigned) const;
    bool checkNodeInPort(unsigned, unsigned) const;
    bool checkNodeOutPort(unsigned, unsigned) const;

    bool registerNode(unsigned, const std::string&, unsigned, unsigned);
    void registerEdge(unsigned, unsigned, unsigned, unsigned);
    void getRoots(term::Term::term_vector&);
public:

    DepParser(NameTableTy& table, DeclTableTy& sigs) : names(table),
    signatures(sigs), nodesDeclared(-1), nodesFound(0), edgesFound(0),
    rootsBeforeCSE(0), nodesAfterCSE(0), rootsAfterCSE(0),
    incompletelySpecifiedTerms(0) {}

    void parse(std::istream&, TermMap&);
    std::ostream& stats(std::ostream&) const;
    void computeStats(TermMap&);
    /*
    unsigned getNumParsedNodes() const {
        return (unsigned)nodesDeclared; 
    }// */

    // Debug
    void printNameTable() const;
};

} // End of the parser namespace

#endif // DEPENDENCY_PARSER_H
