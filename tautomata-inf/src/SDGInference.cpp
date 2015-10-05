// $Id: SDGInference.cpp 444 2011-01-16 07:40:50Z babic $

#include "DependencyParser.h"
#include "KRootHash.h"
#include "CClosure.h"
#include "DotDump.h"
#include "Acceptor.h"
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/bimap/bimap.hpp>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace std;
using namespace boost::program_options;
using namespace boost::filesystem;
using namespace boost::bimaps;
using namespace parser;
using namespace term;

typedef bimap<std::string, unsigned> NameTableTy;
typedef std::pair<unsigned, unsigned> NodeSignatureTy;
typedef utils::Map<int, NodeSignatureTy> DeclTableTy;

int main(int argc, char** argv) {
    options_description opts;
    options_description format("Input parameters");
    options_description misc("Misc options");
    variables_map vm;

    Parser* trainParser = 0;
    Parser* testParser = 0;

    Parser::TermMap trainTerms;
    Parser::TermMap testTerms;
    NameTableTy nametable;
    DeclTableTy signatures;

    format.add_options()
        ("train", value<string>(), 
         "Directory containing sdg training files")
        ("test", value<string>(),
         "Directory containing sdg test files")
        ("testfile", value<string>(),
         "Single sdg file (detection mode)")
        ;
    misc.add_options()
        ("k", value<unsigned>()->default_value((unsigned)2),
         "Learn a k-reversible automaton")
        ("stats", "Print statistics")
        ("draw", value<string>(), 
         "Dump inferred state machine graph in DOT format "
         "to a file")
        ("draw-stdout", "Dump inferred state machine graph in "
         "DOT format to stdout")
        ("help", "Print this message")
        ("printroots", value<int>()->default_value(-1),
         "Print roots deeper than the passed value")
        ;
    opts.add(format).add(misc);

    try {
        store(parse_command_line(argc, argv, opts), vm);
        notify(vm);
    } catch (...) {
        cerr << opts << endl;
        return EXIT_FAILURE;
    }

    if (argc == 1 || vm.count("help")) {
        cout << opts << endl;;
        return EXIT_SUCCESS;
    }

    if (vm.count("testfile")) {
        testParser = new DepParser(nametable, signatures);
        const path fpath(vm["testfile"].as<string>());
        if (!exists(fpath)) {
            cerr << "Can't open the sdg file: " <<
                fpath << endl;
            return EXIT_FAILURE;
        }
        try {
            ifstream in(fpath.string().c_str());
            if (!in) {
                cerr << "Can't open the sdg file: " <<
                    fpath << endl;
                return EXIT_FAILURE;
            }
            testParser->setFilename(fpath.string());
            testParser->parse(in, testTerms);
        } catch (...) {
            cerr << "Can't open the sdg file: " << fpath << endl;
            return EXIT_FAILURE;
        }
    }

    // Parse train and test files
    for (int i = 0; i < 2; i++) {

        if (i == 1 && testParser != 0) {
            break;
        }

        Parser* p = 0;

        if (i == 1 && trainTerms.empty()) {
            // It makes no sense to process the test files without
            // processing the train files
            break;
        }

        const char* option = (i == 0) ? "train" : "test";
        Parser::TermMap& terms = (i == 0) ? trainTerms : testTerms;

        if (vm.count(option)) {
            p = new DepParser(nametable, signatures);

            if (i == 0) {
                trainParser = p;
            } else {
                testParser = p;
            }

            const path dirPath(vm[option].as<string>());
            if (!exists(dirPath)) {
                cerr << "Can't open the directory with sdg " <<
                    option << " files." << endl;
                return EXIT_FAILURE;
            }
            for (recursive_directory_iterator I(dirPath), E; I!=E; ++I){
                if (is_regular_file(I->status())) {
                    const path fileNm = I->path();
                    if (fileNm.extension() == ".sdg") {
                        try {
                            ifstream in(fileNm.string().c_str());
                            if (!in) {
                                cerr << "Can't open the sdg file: " <<
                                    fileNm << endl;
                                continue;
                            }
                            p->setFilename(fileNm.string());
                            p->parse(in, terms);
                        } catch (...) {
                            cerr << "Can't open the input sdg file:" <<
                                fileNm << endl;
                        }
                    }
                }
            }
        } else if (i == 0) {
            cerr << "The --" << option << " option is mandatory." <<
                endl;
            return EXIT_FAILURE;
        }
    }

    if (vm.count("stats")) {
        std::cout << "S ---- Train set ---------------------------"
            << std::endl;
        trainParser->computeStats(trainTerms);
        (void)trainParser->stats(cout);
        if (testParser) {
            std::cout << "S ---- Test set ---------------------------"
                << std::endl;
            testParser->computeStats(testTerms);
            (void)testParser->stats(cout);
        }
    }

    if (!trainTerms.empty()) {
        // Learn k-reversible tree automaton accepting the dependency
        // graphs

        const unsigned cutoff = vm["k"].as<unsigned>();
        Term::term_vector roots;
        Term::term_vector allterms;
        Closure cc;

        for (Parser::TermMap::iterator I = trainTerms.begin(), E =
                trainTerms.end(); I != E; ++I) {
            Term* t = I->second;
            allterms.push_back(t);
            t->clearKLevHash();
            cc.checkin(t);
            if (t->isFinal()) {
                roots.push_back(t);
            }
        }

        const int proots = vm["printroots"].as<int>();
        if (proots > -1) {
            std::cout << "S ---- Printing terms deeper than " << proots
                << " ----" << std::endl;
            for (Term::term_vector::const_iterator I = roots.begin(), E
                    = roots.end(); I != E; ++I) {
                const Term* t = *I;
                if ((int)t->termDepth() >= proots) {
                    std::cout << "Depth= " << t->termDepth() << ": ";
                    t->print(std::cout);
                    std::cout << std::endl << std::endl;
                }
            }
            std::cout << "-----------------------------------------" <<
                std::endl;
        }

        for (unsigned k = 0; k < cutoff; k++) {
            KRootHash().visit(roots);
            /*
            stringstream ss;
            ss << k << "cutoff.dot";
            ofstream out(ss.str().c_str());
            DotDump dd(roots);
            dd.print(out);
            // */
        }

        // An attempt to make things more deterministic
        std::sort(allterms.begin(), allterms.end(), TermSorter());

        // Now build a hash table for matching k-roots
        Parser::TermMap krootMap;
        for (Term::term_vector::iterator I = allterms.begin(), E =
                allterms.end(); I != E; ++I) {
            Term* t = *I;

            Parser::TermMap::const_iterator_pair CIP =
                krootMap.equal_range(t->getKLevHash());
            bool found = false;
            for (; CIP.first != CIP.second; ++CIP.first) {
                Term* x = CIP.first->second;
                if (t->klevEqual(*x, cutoff)) {
                    assert(t->termDepth() > cutoff &&
                        "If equal, terms should have been "
                        "merged during CSE.");
                    cc.merge(t, x);
                    found = true;
                    break;
                }
            }
            if (!found) {
                krootMap.insert(std::make_pair(t->getKLevHash(), t));
            }
        }

        // Compute congruence closure
        cc.propagate();
        // Build the automaton
        cc.rebuildAutomaton(nametable.size(), trainTerms);

        // At this point, the trainTerms hash table contains possibly
        // cyclic term graph that actually describes a tree automaton.
        // The leaf subtrees that contain no introduced states still
        // have correct hashes, and we use that property to bootstrap
        // the acceptance check.

        if (vm.count("stats")) {
            std::cout << "S ---- Train set automaton "
                "-----------------" << std::endl;
            (void)cc.stats(cout);
        }
    }

    if (!testTerms.empty()) {
        // Run the test graphs against the inferred automaton
        Acceptor acc(trainTerms);
        for (Parser::TermMap::iterator I = testTerms.begin(), E =
                testTerms.end(); I != E; ++I) {
            Term* t = I->second;
            if (t->isFinal()) {
                acc.visit(t);
            }
        }

        if (vm.count("stats")) {
            (void)acc.stats(cout);
        }
    }

    Parser::deleteNodes(trainTerms);
    Parser::deleteNodes(testTerms);
    delete trainParser;
    delete testParser;
    return EXIT_SUCCESS;
}
