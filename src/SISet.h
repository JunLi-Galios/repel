/*
 * SISet.h
 *
 *  Created on: May 24, 2011
 *      Author: joe
 */

#ifndef SISET_H_
#define SISET_H_
#include <set>
#include <list>
#include <iostream>
#include "SpanInterval.h"
#include <boost/functional/hash.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/list.hpp>

class SISet {
public:
    SISet(bool forceLiquid=false,
            const Interval& maxInterval=Interval(0,0))
    : set_(), forceLiquid_(forceLiquid), maxInterval_(maxInterval) {}

    SISet(const SpanInterval& si, bool forceLiquid,
          const Interval& maxInterval);

    template <class InputIterator>
    SISet(InputIterator begin, InputIterator end,
            bool forceLiquid,
            const Interval& maxInterval)
    : set_(begin, end), forceLiquid_(forceLiquid), maxInterval_(maxInterval) {}

    typedef std::list<SpanInterval>::const_iterator const_iterator;

    const_iterator begin() const;
    const_iterator end() const;

    std::set<SpanInterval> asSet() const;   /** DEPRECATED */
    bool forceLiquid() const {return forceLiquid_;};
    // TODO make some of these friend functions
    bool isDisjoint() const;
    SISet compliment() const;
    Interval maxInterval() const;
    unsigned int size() const;
    unsigned int liqSize() const;
    bool empty() const;
    const std::list<SpanInterval>& intervals() const {return set_;}

    // modifiers
    void add(const SpanInterval &s);
    void add(const SISet& b);

    void makeDisjoint();
    void clear() {set_.clear();};
    void setMaxInterval(const Interval& maxInterval);
    void setForceLiquid(bool forceLiquid);
    void subtract(const SpanInterval& si);
    void subtract(const SISet& sis);

    /**
     * Check to see if this SISet includes another SISet.  This is equivalent
     * to set inclusion.
     *
     * @param s SISet to check to see if it's included
     * @return true if s is in this, false otherwise
     */
    bool includes(const SISet& s) const;
    bool includes(const SpanInterval& si) const;
    bool includes(const Interval& interval) const;

    const SISet satisfiesRelation(const Interval::INTERVAL_RELATION& rel) const;

    /**
     * Construct a SISet from another where the
     * representation is normalized (made as compact as possible).
     *
     * @param set The SISet to normalize.
     * @return A SISet representing the same set of intervals but normalized (compact).
     */
 //   static SISet asNormalized(const SISet& set);
    static SISet randomSISet(bool forceLiquid, const Interval& maxInterval, boost::mt19937& rng);
    SpanInterval randomSI(boost::mt19937& rng) const;

    template<class OutIter>
    void collectSegments(OutIter out) const;

    std::string toString() const;
    friend std::ostream& operator<<(std::ostream& o, const SISet& s);
    friend bool operator ==(const SISet& l, const SISet& r);
    friend bool operator !=(const SISet& l, const SISet& r);

    friend SISet intersection(const SISet& a, const SISet& b);
    friend SISet intersection(const SISet& a, const SpanInterval& si);
    friend SISet span(const SpanInterval& a, const SpanInterval& b, const Interval& maxInterval);
    friend bool equalByInterval(const SISet& a, const SISet& b);
    friend std::size_t hash_value(const SISet& si);
private:
    friend class boost::serialization::access;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);

    std::list<SpanInterval> set_;
    bool forceLiquid_;
    Interval maxInterval_;
};


struct SISetInserterHelper {
    SISetInserterHelper(SISet *s) : siset(s), nogood(false) {}

    SISetInserterHelper& operator=(const SpanInterval& si) {
        if (siset == 0)
            throw std::logic_error("can't add a spanning interval to an inserter created with null ptr reference");
        siset->add(si);
        nogood = true;
        return *this;
    }

    SISet *siset;
    bool nogood;
};

// output iterator for SISet
class SISetInserter : public std::iterator<std::output_iterator_tag, SpanInterval> {
public:
    SISetInserter() : siset(0), helper(0) {}
    SISetInserter(SISet& s) : siset(&s), helper(&s) {}

    SISetInserterHelper& operator*() {
        return helper;
    }

    SISetInserter& operator++() {
        helper.nogood = false;
        return *this;
    }

    SISetInserter operator++(int) {
        SISetInserter copy(*this);
        helper.nogood = false;
        return copy;
    }
private:
    SISet *siset;
    SISetInserterHelper helper;
};

SISet span(const SpanInterval& a, const SpanInterval& b, const Interval& maxInterval);
SISet composedOf(const SpanInterval& a, const SpanInterval& b, Interval::INTERVAL_RELATION, const SpanInterval& universe);
bool equalByInterval(const SISet& a, const SISet& b);

unsigned long hammingDistance(const SISet& a, const SISet& b);

// IMPLEMENTATION
inline SISet::const_iterator SISet::begin() const {return set_.begin();}
inline SISet::const_iterator SISet::end() const {return set_.end();}
inline bool SISet::empty() const { return size() == 0;}


inline bool SISet::includes(const SISet& s) const {
    SISet copy = s;
    copy.subtract(*this);
    return copy.empty();
}

inline bool SISet::includes(const SpanInterval& si) const {
    SISet onlySi(false, maxInterval_);
    onlySi.add(si);
    return includes(onlySi);
}

inline bool SISet::includes(const Interval& interval) const {
    return includes(SpanInterval(interval.start(), interval.start(), interval.finish(), interval.finish()));
}

//inline bool operator==(const SISet& l, const SISet& r) {return l.includes(r) && r.includes(l);}    //TODO: is this the right thing to do???
inline bool operator==(const SISet& l, const SISet& r) {return l.set_ == r.set_;}
inline bool operator!=(const SISet& l, const SISet& r) {return !operator==(l,r);}

inline std::size_t hash_value(const SISet& si) {
    std::size_t seed = 0;
    // make a copy of our set as a liquid set (inclusive)
    SISet liqSet(true, si.maxInterval_);
    for (SISet::const_iterator it = si.begin(); it != si.end(); it++) {
        liqSet.add(it->toLiquidInc());
    }
    boost::hash_range(seed, liqSet.begin(), liqSet.end());
    return seed;
}

template<class OutIter>
void SISet::collectSegments(OutIter out) const {

    std::copy(begin(), end(), out);
    SISet comp = compliment();
    std::copy(comp.begin(), comp.end(), out);
}

template <class Archive>
void SISet::serialize(Archive& ar, const unsigned int version) {
    ar & set_;
    ar & forceLiquid_;
    ar & maxInterval_;
}

#endif
