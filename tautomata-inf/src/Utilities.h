// $Id: Utilities.h 356 2011-01-05 00:10:50Z babic $

#ifndef UTILITIES_H
#define UTILITIES_H

#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <cstddef>
#include <functional>

namespace utils {

// The two classes below require specialization of std::tr1::hash and
// working operator== (or specialization of std::equal_to)

template <class K, class T, class H = std::tr1::hash<K> >
class Multimap : public std::tr1::unordered_multimap<K,T,H> {
    typedef typename std::tr1::unordered_multimap<K,T,H> _Base;
public:
    typedef typename _Base::iterator iterator;
    typedef typename _Base::const_iterator const_iterator;
    typedef std::pair<iterator,iterator> iterator_pair;
    typedef std::pair<const_iterator,const_iterator>
        const_iterator_pair;
};

template <class K, class T, class H = std::tr1::hash<K> >
class Map : public std::tr1::unordered_map<K,T,H> {
    typedef typename std::tr1::unordered_map<K,T,H> _Base;
public:
    typedef typename _Base::iterator iterator;
    typedef typename _Base::const_iterator const_iterator;
    typedef std::pair<iterator,iterator> iterator_pair;
    typedef std::pair<const_iterator,const_iterator>
        const_iterator_pair;
};

template <class T, class H = std::tr1::hash<T> >
class Multiset : public std::tr1::unordered_multiset<T,H> {
    typedef typename std::tr1::unordered_multiset<T,H> _Base;
public:
    typedef typename _Base::iterator iterator;
    typedef typename _Base::const_iterator const_iterator;
    typedef std::pair<iterator,iterator> iterator_pair;
    typedef std::pair<const_iterator,const_iterator>
        const_iterator_pair;
};

template <class T, class H = std::tr1::hash<T> >
class Set : public std::tr1::unordered_set<T,H> {
    typedef typename std::tr1::unordered_set<T,H> _Base;
public:
    typedef typename _Base::iterator iterator;
    typedef typename _Base::const_iterator const_iterator;
    typedef std::pair<iterator,iterator> iterator_pair;
    typedef std::pair<const_iterator,const_iterator>
        const_iterator_pair;
};

template <typename T>
struct Hash {
    size_t operator()(T e) const {
        return static_cast<std::size_t>(e);
    }   
};

template <typename T>
struct Hash<const T *const> {
    std::size_t operator()(const T *const x) const {
        return reinterpret_cast<std::size_t>(x);
    }
};

/* Usage example:
std::transform(m.begin(), m.end(), std::back_inserter(vk),
               select1st<std::map<K, I>::value_type>()) ; 
// */

// Select 1st from the pair.
template <typename Pair>
struct select1st {
    typedef typename Pair::first_type result_type;
    const result_type &operator()(const Pair &p) const { 
        return p.first; 
    }   
};

// Select 2nd from the pair.
template <typename Pair>
struct select2nd {
    typedef typename Pair::second_type result_type ;
    const result_type &operator()(const Pair &p) const { 
        return p.second; 
    }   
}; 

// Associative inserter (for instance inserting into sets).
template <class Container>
class asso_inserter : public 
    std::iterator <std::output_iterator_tag, void, void, void, void> {
protected:
    Container& container;    // container in which elements are inserted
public:
    explicit asso_inserter (Container& c) : container(c) {}

    // assignment operator
    // - inserts a value into the container
    asso_inserter<Container>&
    operator=(const typename Container::value_type& value) { 
        container.insert(value);
        return *this;
    }   

    // dereferencing is a no-op that returns the iterator itself
    asso_inserter<Container>& operator*() { return *this; }
    // increment operation is a no-op that returns the iterator itself
    asso_inserter<Container>& operator++() { return *this; }
    asso_inserter<Container>& operator++(int) { return *this; }
};

unsigned gcdSafe(unsigned, unsigned);
int lcm(int, int);
int tlz(unsigned);

template <typename T>
inline const T& min(const T& x, const T& y) {
    return x < y ? y : x;
}

template <typename T>
inline const T& max(const T& x, const T& y) {
    return x < y ? y : x;
}

template<typename T>
struct maximum : public std::binary_function<T, T, T> {
    const T& operator()(const T& x, const T& y) const {
        return x <  y ? y : x;
    }
};

template<typename T>
struct minimum : public std::binary_function<T, T, T> {
    const T& operator()(const T& x, const T& y) const {
        return x < y ? x : y;
    }
};

template<typename T>
struct bitwise_and : public std::binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const {
        return x & y;
    }
};

template<typename T>
struct bitwise_or : public std::binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const {
        return x | y;
    }
};

template<typename T>
struct logical_xor : public std::binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const {
        return (x || y) && !(x && y);
    }
};

template<typename T>
struct bitwise_xor : public std::binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const {
        return x ^ y;
    }
};

} // End of utils namespace

#endif // UTILITIES_H
