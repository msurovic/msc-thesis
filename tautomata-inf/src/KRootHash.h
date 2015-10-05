// $Id: KRootHash.h 342 2010-12-28 01:32:01Z babic $

#ifndef KHASH_ROOT_H
#define KHASH_ROOT_H

#include "TermVisitor.h"

namespace term {

class KRootHash : public BFSVisitor {
public:
    void visitNode(Term* t) { t->recomputeKLevHash(); }
};

} // End of the term namespace

#endif // KHASH_ROOT_H
