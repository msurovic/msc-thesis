// $Id: Checker.cpp 376 2011-01-09 04:00:47Z babic $

#include "DependencyParser.h"
#include "DotDump.h"
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/bimap/bimap.hpp>
#include <fstream>
#include <string>
#include <iostream>

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
    Parser::TermMap terms;
    NameTableTy nametable;
    DeclTableTy signatures;

    format.add_options()
        ("sdg", value<string>(), "Graph file to analyze in sdg format")
        ("sdg-stdin", "Read graph from stdin in sdg format")
        ("sdg-dir", value<string>(), 
         "Process all sdg files in directory (recursively)")
        ;
    misc.add_options()
        ("stats", "Print statistics")
        ("draw", value<string>(), 
         "Dump parsed graph in DOT format to file")
        ("draw-stdout", "Dump parsed graph in DOT format to stdout")
        ("help", "Print this message")
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

    Parser* p = new DepParser(nametable, signatures);

    {
        if (vm.count("sdg-stdin")) {
            p->parse(cin, terms);
        } else if (vm.count("sdg")) {
            try {
                ifstream in(vm["sdg"].as<string>().c_str());
                if (!in) {
                    cerr << "Can't open the input sdg file." << endl;
                    return EXIT_FAILURE;
                }
                p->parse(in, terms);
            } catch (...) {
                cerr << "Can't open the input sdg file." << endl;
                return EXIT_FAILURE;
            }
        } else if (vm.count("sdg-dir")) {
            const path dirPath(vm["sdg-dir"].as<string>());
            if (!exists(dirPath)) {
                cerr << "Can't open the directory with sdg files." <<
                    endl;
                return EXIT_FAILURE;
            }
            for (recursive_directory_iterator I(dirPath), E; I!=E; ++I){
                if (is_regular_file(I->status())) {
                    const path fileNm = I->path();
                    if (fileNm.extension() == ".sdg") {
                        try {
                            ifstream in(fileNm.string().c_str());
                            if (!in) {
                                cerr << "Can't open sdg file: " <<
                                    fileNm << endl;
                                continue;
                            }
                            //std::cerr << fileNm << std::endl;
                            p->setFilename(fileNm.string());
                            p->parse(in, terms);
                        } catch (...) {
                            cerr << "Can't open the input sdg file:" <<
                                fileNm << endl;
                        }
                    }
                }
            }
        }

        if (!terms.empty()) { // Something parsed
            if (vm.count("stats")) {
                p->computeStats(terms);
                (void)p->stats(cout);
            }
        }
    }

    if (vm.count("draw") || vm.count("draw-stdout")) {
        Term::term_vector roots;
        Parser::getRoots(terms, roots);
        if (vm.count("draw")) {
            try {
                ofstream outf(vm["draw"].as<string>().c_str());
                if (!outf) {
                    cerr << "Can't create the output dot file." << endl;
                    return EXIT_FAILURE;
                }
                DotDump(roots).print(outf);
            } catch (...) {
                cerr << "Can't create the output dot file." << endl;
                return EXIT_FAILURE;
            }
        } else if (vm.count("draw-stdout")) {
            DotDump(roots).print(cout);
        }
    }

    Parser::deleteNodes(terms);
    delete p;
    return EXIT_SUCCESS;
}
