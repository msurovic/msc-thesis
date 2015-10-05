// $Id: DependencyParser.cpp 405 2011-01-12 10:07:13Z babic $

#include "DependencyParser.h"
#include "CSE.h"
#include "DotDump.h"
#include <string>
#include <sstream>

using namespace parser;
using namespace term;
    
#define PERROR(M) perror(std::cerr, line, M)
#define PWARN(M) pwarn(std::cerr, line, M)

bool DepParser::checkNodeId(unsigned id) const {
    return signatures.find(id) != signatures.end();
}

bool DepParser::checkNodeInPort(unsigned id, unsigned port) const {
    DeclTableTy::const_iterator I = signatures.find(id);
    if (I == signatures.end()) {
        return false;
    } else {
        return port < I->second.first;
    }
}

/*
bool DepParser::checkNodeOutPort(unsigned id, unsigned port) const {
    DeclTableTy::const_iterator I = signatures.find(id);
    if (I == signatures.end()) {
        return false;
    } else {
        return port < I->second.second;
    }
}// */

bool DepParser::registerNode(unsigned locid, const std::string& name,
        unsigned inArity, unsigned outArity) {

    unsigned globid = 0;
    NameTableTy::left_const_iterator LI = names.left.find(name);
    const std::string* nPtr = 0;
    if (LI == names.left.end()) {

        // Even though the name is new, the ID might have already been
        // used by some previously parsed benchmark
        NameTableTy::right_const_iterator RI = names.right.find(locid);
        globid = names.size();
        assert(names.right.find(globid) == names.right.end() &&
                "Creating a duplicate ID?");
        assert(globid == names.size() && "Unexpected condition.");
        signatures[globid] = std::make_pair(inArity, outArity);
        names.insert(NameTableTy::value_type(name, globid));
        nPtr = &names.left.find(name)->first;
    } else {
        globid = LI->second;
        const NodeSignatureTy& sig = signatures[globid];

        // In SDG files, the same function names are sometimes used for
        // differnt terms, more precisely, terms with a different number
        // of operands. That creates problems because names are supposed
        // to be unique identifiers. This code here is a workaround.

        // Arity mismatch, need to create a new name. Mismatch in the
        // output arity doesn't matter, as it's ignored by the tool.
        if (inArity != sig.first) {
            std::string newName = name + '_';
            
            /* Enable this to get warnings about 

            std::stringstream ss;
            ss << "Node " << locid << " (" << name << 
                ") " << inArity << ':' << outArity << 
                " previously declared with arity: " <<
                signatures[names.left.find(name)->second].first
                << ':' <<
                signatures[names.left.find(name)->second].second
                << ". Renaming the node to: " << newName << std::endl;
            PWARN(ss.str().c_str());
            // */

            return registerNode(locid, newName, inArity, outArity);
        }

        nPtr = &LI->first;
    }

    local[locid] = globid;
    partialDefs[locid] = new Term(nPtr, Term::term_vector(inArity,
                0), globid);
    return true;
}

void DepParser::registerEdge(unsigned locsrc, unsigned, unsigned
        locdst, unsigned dstPort) {
    assert(partialDefs[locsrc] && partialDefs[locdst] &&
            "Unknown src/dst node.");
    Term& srcTrm = *partialDefs[locsrc];
    Term& dstTrm = *partialDefs[locdst];
    dstTrm[dstPort] = &srcTrm;
    srcTrm.addUse(&dstTrm);
}

void DepParser::getRoots(Term::term_vector& roots) {
    for (TermTableTy::const_iterator I = partialDefs.begin(), E =
            partialDefs.end(); I != E; ++I) {
        Term* t = *I;
        if (t && t->isFinal()) {
            roots.push_back(t);
        }
    }
}

void DepParser::parse(std::istream& is, TermMap& tm) {
    char nxt = ' ';
    line = 1;

    unsigned nodeId = 0;
    unsigned inArity = 0;
    unsigned outArity = 0;
    std::string varName;

    unsigned from = 0;
    unsigned to = 0;
    unsigned fromPort = 0;
    unsigned toPort = 0;

    while (is >> nxt) {
        eatSpace(is);
        switch (nxt) {
            case '\n': // Empty line
            case '#' : // Comment
                eatLine(is); 
                line++;
                break;
            case 'N' : // Number of nodes
                is >> nodesDeclared;
                eatSpace(is);
                if (!is.good() || nodesDeclared < 0) {
                    PERROR("Invalid number of nodes.");
                }
                eatLine(is);
                TermTableTy(nodesDeclared+1, (Term*)0).swap(partialDefs);
                line++;
                break;
            case 'V' : // Node declaration
                if (nodesDeclared < 0) {
                    PERROR("Number of nodes (N line) has to be "
                        "declared before the first node declaration.");
                }
                eatSpace(is);
                is >> nodeId;
                if (!is.good() || is.eof()) {
                    PERROR("Node id expected.");
                }
                if (nodeId > (unsigned)nodesDeclared) {
                    PERROR("Node ID greater than the declared number "
                            "of nodes.");
                }
                eatSpace(is);
                is >> varName;
                if (!is.good() || is.eof()) {
                    PERROR("Node name expected.");
                }
                is >> inArity;
                if (!is.good() || is.eof()) {
                    PERROR("Node IN arity expected.");
                }
                is >> outArity;
                if (!is.good() || is.eof()) {
                    PERROR("Node OUT arity expected.");
                }
                eatLine(is);
                line++;
                nodesFound++;
                if (!registerNode(nodeId, varName, inArity, outArity)) {
                    PERROR("Node processing error.");
                }
                break;
            case 'E' : // Edge declaration
                eatSpace(is);
                is >> from;
                if (!is.good() || is.eof()) {
                    PERROR("Source node expected.");
                }
                if (!checkNodeId(local[from])) {
                    PERROR("Invalid source node ID.");
                }
                eatSpace(is);
                nxt = is.get();
                if (nxt != ':' || !is.good() || is.eof()) {
                    PERROR("':' delimiter expected.");
                }
                eatSpace(is);
                is >> fromPort;
                if (!is.good() || is.eof()) {
                    PERROR("Source port expected.");
                }
                /*
                if (!checkNodeOutPort(local[from], fromPort)) {
                    PERROR("Invalid source port.");
                }// */
                eatSpace(is);
                nxt = is.get();
                if (nxt != ',' || !is.good() || is.eof()) {
                    PERROR("',' separator expected.");
                }
                eatSpace(is);
                is >> to;
                if (!is.good() || is.eof()) {
                    PERROR("Destination node expected.");
                }
                if (!checkNodeId(local[to])) {
                    PERROR("Invalid destination node ID.");
                }
                eatSpace(is);
                nxt = is.get();
                if (nxt != ':' || !is.good() || is.eof()) {
                    PERROR("':' delimiter expected.");
                }
                eatSpace(is);
                is >> toPort;
                if (!is.good() || is.eof()) {
                    PERROR("Destination port expected.");
                }
                if (!checkNodeInPort(local[to], toPort)) {
                    PERROR("Invalid destination port.");
                }
                eatLine(is);
                line++;
                edgesFound++;
                registerEdge(from, fromPort, to, toPort);
                break;
            default:
                PERROR("Unrecognized line type.");
                break;
        }
    }

    if (nodesDeclared == -1) {
        PERROR("Missing the node number (N) line.");
    }

    /* This creates problems when multiple files are parsed at once
    if ((unsigned)nodesDeclared != nodesFound) {
        PERROR("The N line does not match the number of "
                "nodes found.");
    }// */

    // Eliminate common subexpressions
    {
        Term::term_vector roots;
        getRoots(roots);
        rootsBeforeCSE = roots.size();

        CSE cse(tm);
        try {
            for (Term::term_vector::const_iterator I = roots.begin(), E
                    = roots.end(); I != E; ++I) {

                bool defined = false;
                {
                    FullyDefined fd;
                    fd.visit(*I);
                    defined = fd.isFullyDefined();
                }

                if (defined) {
                    cse.visit(*I);
                } else {
                    incompletelySpecifiedTerms++;
                }
            }
        } catch (MissingEdge& me) {
            std::stringstream ss;
            ss << "Node " << me.getId() << " is missing " <<
                me.getIdx() << " parameter";
            PERROR(ss.str().c_str());
        }
    }

    // Cleanup temporary data structures
    for (TermTableTy::const_iterator I = partialDefs.begin(), E =
            partialDefs.end(); I != E; ++I) {
        delete *I;
    }
    local.clear();
    TermTableTy().swap(partialDefs);
}

#undef PERROR

std::ostream& DepParser::stats(std::ostream& os) const {
    os << "S Total nodes:      " << nodesFound << std::endl;
    os << "S Total edges:      " << edgesFound << std::endl;
    os << "S Roots before CSE: " << rootsBeforeCSE << std::endl;
    os << "S Nodes after CSE:  " << nodesAfterCSE << std::endl;
    os << "S Roots after CSE:  " << rootsAfterCSE << std::endl;
    os << "S Incomplete terms: " << incompletelySpecifiedTerms <<
        std::endl;
    os << "S ------------------------------------------" << std::endl;
    for (StatsTableTy::const_iterator I = depthDistribution.begin(), E =
            depthDistribution.end(); I != E; ++I) {
        os << "S Terms with depth " << I->first << ": " << I->second <<
            std::endl;
    }
    os << "S ------------------------------------------" << std::endl;
    return os;
}

void DepParser::computeStats(TermMap& tm) {
    Term::term_vector roots;
    nodesAfterCSE = tm.size();
    Parser::getRoots(tm, roots);
    rootsAfterCSE = roots.size();
    for (Term::term_vector::const_iterator I = roots.begin(), E
            = roots.end(); I != E; ++I) {
        const int dpth = (*I)->termDepth();
        StatsTableTy::iterator SI = depthDistribution.find(dpth);
        if (SI == depthDistribution.end()) {
            depthDistribution[dpth] = 1;
        } else {
            SI->second++;
        }
    }
}

void DepParser::printNameTable() const {
    for (NameTableTy::left_const_iterator I = names.left.begin(), E =
            names.left.end(); I != E; ++I) {
        std::cout << I->first << " : " << I->second << std::endl;
    }
}
