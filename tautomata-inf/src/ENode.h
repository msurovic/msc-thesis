// $Id: ENode.h 355 2011-01-04 00:28:49Z babic $

#ifndef EQUIVALENCE_NODE_H
#define EQUIVALENCE_NODE_H

#include <vector>
#include <algorithm>
#include <cassert>
#include <iostream>

namespace enode {

template <typename T>
class ENode {
    typedef ENode<T> _Self;

    _Self* parent;
    T* val;
    std::vector<_Self*> eClass;

public:
    typedef typename std::vector<_Self*>::iterator iterator;

    iterator begin() { return eClass.begin(); }
    iterator end() { return eClass.end(); }

    ENode(T* value) : parent(0), val(value) {
        eClass.push_back(this);
        assert(check() && "Invalid ENode created.");
    }

    T* getVal() const { return val; }

    _Self* getParent() const { return parent; }
    void setParent(_Self* p) { parent = p; }

    _Self* getRepresentative() {
        if (parent) { return parent->getRepresentative(); }
        else return this;
    }
    
    const _Self* getRepresentative() const {
        if (parent) { return parent->getRepresentative(); }
        else return this;
    }

    bool isClassRepresentative() const {
        return !getParent();
    }

    unsigned getEquivalenceClassSize() const { 
        return eClass.size();
    }

    // Merge two equivalence classes
    _Self& operator+=(ENode& e) {
        assert(check() && "Invalid ENode.");
        assert(e.check() && "Invalid ENode.");
        assert(!e.getParent() && "Reconnecting the parent?");
        assert(!getParent() && 
                "Merging with a non-class representative?");
        assert(e <= *this && 
            "Performance bug: merging larger into smaller class?");
        assert(&e != this && "Implementation of equivalence "
                "class join not reflective.");
        assert(getEquivalenceClassSize() > 0 && 
                "Invalid eq class size.");

        for (typename std::vector<_Self*>::iterator I =
                e.eClass.begin(), E = e.eClass.end(); I != E; ++I) {
            eClass.push_back(*I);
            assert(*I != this && 
                "Creating a cycle in union-find data structure?");
            (*I)->setParent(this); // Eager path compression
        }

        std::vector<_Self*>().swap(e.eClass);

        assert(!getParent() &&
                "Incorrect parent change.");
        assert(e.getParent() == this &&
                "Invalid representative modification.");
        assert(getEquivalenceClassSize() > 0 && 
                "Invalid eq class size.");
        assert(check() && "Invalid ENode.");
        assert(e.check() && "Invalid ENode.");

        return *this;
    }

    bool operator==(const ENode& e) const {
        return getRepresentative() == e.getRepresentative();
    }

    bool operator!=(const ENode& e) const {
        return getRepresentative() != e.getRepresentative();
    }

    bool operator<=(const ENode& e) const {
        return getEquivalenceClassSize() <= e.getEquivalenceClassSize();
    }

    // Debug
    bool check() const {
        if (eClass.empty()) {
            return !isClassRepresentative();
        }

        bool inItsOwnEqClass = false;
        for (typename std::vector<_Self*>::const_iterator I =
                eClass.begin(), E = eClass.end(); I != E; ++I) {
            const ENode* e = *I;
            if (e == this) {
                inItsOwnEqClass = true;
            }
        }

        return inItsOwnEqClass;
    }
};

} // End of the enode namespace

#endif // EQUIVALENCE_NODE_H
