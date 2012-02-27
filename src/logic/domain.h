/*
 * domain.h
 *
 *  Created on: May 20, 2011
 *      Author: joe
 */

#ifndef DOMAIN_H_
#define DOMAIN_H_

#include <set>
#include <string>
#include <map>
#include <utility>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <boost/optional.hpp>
#include <boost/unordered_set.hpp>
#include "el_syntax.h"
#include "predcollector.h"
#include "model.h"
#include "../siset.h"
#include "namegenerator.h"
#include "../lrucache.h"
#include "../utils.h"

std::string modelToString(const Model& m);

// TODO: this is quite a bad way to handle closed worlds.  Redo this entire mechanic so model's account for this, not the domain.

class Domain {
public:
    typedef std::vector<ELSentence>::const_iterator formula_const_iterator;
    typedef boost::unordered_set<Atom>::const_iterator obsAtoms_const_iterator;
    typedef boost::unordered_set<Atom>::const_iterator unobsAtoms_const_iterator;
    typedef MergedIterator<obsAtoms_const_iterator, unobsAtoms_const_iterator, const Atom> atoms_const_iterator;

    Domain();
    template <class FactsForwardIterator>
    Domain(FactsForwardIterator factsBegin, FactsForwardIterator factsEnd,
            const std::vector<ELSentence>& formSet,
            bool assumeClosedWorld=true);
    virtual ~Domain();
    //const FormulaList& formulas() const;

    formula_const_iterator formulas_begin() const;
    formula_const_iterator formulas_end() const;
    obsAtoms_const_iterator obsAtoms_begin() const;
    obsAtoms_const_iterator obsAtoms_end() const;
    unobsAtoms_const_iterator unobsAtoms_begin() const;
    unobsAtoms_const_iterator unobsAtoms_end() const;
    atoms_const_iterator atoms_begin() const;
    atoms_const_iterator atoms_end() const;

    void clearFormulas();
    void addFormula(const ELSentence& e);

    void addObservedPredicate(const Atom& a);
    const std::map<std::string, SISet>& observedPredicates() const; // TODO RENAME
    SISet getModifiableSISet(const std::string& name) const;
    SISet getModifiableSISet(const std::string& name, const SISet& where) const;
    void unsetAtomAt(const std::string& name, const SISet& where);
    NameGenerator& nameGenerator();
    Model defaultModel() const;
    Model randomModel() const;
    Interval maxInterval() const;
    SpanInterval maxSpanInterval() const;
    SISet maxSISet() const;

    void setMaxInterval(const Interval& maxInterval);

    bool isLiquid(const std::string& predicate) const;
    bool dontModifyObsPreds() const;
    bool assumeClosedWorld() const;
    void setDontModifyObsPreds(bool b);
    void setAssumeClosedWorld(bool b);

    unsigned long score(const ELSentence& s, const Model& m) const;
    unsigned long score(const Model& m) const;

    bool isFullySatisfied(const Model& m) const;
private:
    typedef std::pair<const Model*, const Sentence*> ModelSentencePair;

    bool assumeClosedWorld_;
    bool dontModifyObsPreds_;

    std::map<std::string, SISet> obsPredsFixedAt_;
    boost::unordered_set<Atom> obsPreds_;
    boost::unordered_set<Atom> unobsPreds_;
    Interval maxInterval_;

    std::vector<ELSentence> formulas_;
    Model observations_;

    NameGenerator generator_;

    struct ModelSentencePair_cmp : public std::binary_function<ModelSentencePair, ModelSentencePair, bool> {
        bool operator() (const ModelSentencePair& a, const ModelSentencePair& b) const {
            if (a.first < b.first) return true;
            if (a.first > b.first) return false;
            if (a.second < b.second) return true;
            return false;
        }
    };

    //mutable LRUCache<ModelSentencePair,SISet,ModelSentencePair_cmp> cache_;
};

// IMPLEMENTATION
inline Domain::Domain()
    : assumeClosedWorld_(true),
      dontModifyObsPreds_(true),
      obsPredsFixedAt_(),
      obsPreds_(),
      unobsPreds_(),
      maxInterval_(0,0),
      formulas_(),
      observations_(),
      generator_(){};

template <class FactsForwardIterator>
Domain::Domain(FactsForwardIterator factsBegin, FactsForwardIterator factsEnd,
        const std::vector<ELSentence>& formSet,
        bool assumeClosedWorld)
        : assumeClosedWorld_(assumeClosedWorld),
          dontModifyObsPreds_(true),
          obsPredsFixedAt_(),
          obsPreds_(),
          unobsPreds_(),
          maxInterval_(0,0),
          formulas_(formSet),
          observations_(),
          generator_() {

    // find the maximum interval of time
    if (factsBegin == factsEnd) {
        // what to do for time??
        std::runtime_error e("no facts given: currently need at least one fact to determine the interval to reason over!");
        throw e;
    }
    unsigned int smallest=std::numeric_limits<unsigned int>::max(), largest=std::numeric_limits<unsigned int>::min();
    for (FactsForwardIterator it = factsBegin; it != factsEnd; it++) {
        SpanInterval interval = it->where();

        boost::optional<SpanInterval> norm = interval.normalize();
        if (!norm) {
            continue;
        }
        interval = norm.get();
        smallest = (std::min)(interval.start().start(), smallest);
        largest = (std::max)(interval.finish().finish(), largest);
    }
    for (std::vector<ELSentence>::const_iterator it = formulas_.begin(); it != formulas_.end(); it++) {
        if (!it->isQuantified()) continue;
        SISet set = it->quantification();
        for (SISet::const_iterator it2 = set.begin(); it2 != set.end(); it2++) {
            smallest = (std::min)(it2->start().start(), smallest);
            largest = (std::max)(it2->finish().finish(), largest);
        }
    }
    maxInterval_ = Interval(smallest, largest);

    // collect all fact predicates
    for (FactsForwardIterator it = factsBegin; it != factsEnd; it++) {
        boost::shared_ptr<const Atom> a = it->atom();
        SpanInterval si = it->where();

        if (obsPredsFixedAt_.find(a->name()) == obsPredsFixedAt_.end()) {
            SISet newSet(true);       // TODO: assumes all unobs are liquid!!!
            newSet.add(si);
            obsPredsFixedAt_.insert(std::pair<std::string, SISet>(a->name(), newSet));

            // add it as an observed predicate
            obsPreds_.insert(*a);
        } else {
            SISet curSet = obsPredsFixedAt_.find(a->name())->second;
            curSet.add(si);
            obsPredsFixedAt_.erase(a->name());
            obsPredsFixedAt_.insert(std::pair<std::string, SISet>(a->name(), curSet));
        }
    }
    // now collect all unobserved preds

    PredCollector predCollector;
    for (std::vector<ELSentence>::const_iterator it = formulas_.begin(); it != formulas_.end(); it++) {
        it->sentence()->visit(predCollector);
    }

    // remove the predicates we know are observed
    /*
    std::set<std::string> obsJustPreds;
    for (std::map<std::string, SISet>::const_iterator it = obsPredsFixedAt_.begin();
            it != obsPredsFixedAt_.end();
            it++) {
        obsJustPreds.insert(it->first);
    }*/
    for (std::set<Atom, atomcmp>::const_iterator it = predCollector.preds.begin(); it != predCollector.preds.end(); it++) {
        if (obsPreds_.find(*it) == obsPreds_.end()) {
            unobsPreds_.insert(*it);
        }
    }

    // initialize observations
    for (FactsForwardIterator it = factsBegin; it != factsEnd; it++) {
        boost::shared_ptr<const Atom> atom = it->atom();
        SpanInterval interval = it->where();

        // TODO: we are hardwired for liquidity, come back and fix this later

        if (it->truthVal()) {
            SISet set(true);

            set.add(interval);
            observations_.setAtom(*atom, set);
        }
    }
};

inline Domain::~Domain() {};

inline Domain::formula_const_iterator Domain::formulas_begin() const {return formulas_.begin();}
inline Domain::formula_const_iterator Domain::formulas_end() const {return formulas_.end();}
inline Domain::obsAtoms_const_iterator Domain::obsAtoms_begin() const {return obsPreds_.begin();}
inline Domain::obsAtoms_const_iterator Domain::obsAtoms_end() const {return obsPreds_.end();}
inline Domain::unobsAtoms_const_iterator Domain::unobsAtoms_begin() const {return unobsPreds_.begin();}
inline Domain::unobsAtoms_const_iterator Domain::unobsAtoms_end() const {return unobsPreds_.end();}
inline Domain::atoms_const_iterator Domain::atoms_begin() const {
    return atoms_const_iterator(obsPreds_.begin(), obsPreds_.end(), unobsPreds_.begin(), unobsPreds_.end());
}
inline Domain::atoms_const_iterator Domain::atoms_end() const {
    return atoms_const_iterator(obsPreds_.end(), obsPreds_.end(), unobsPreds_.end(), unobsPreds_.end());
}

inline void Domain::clearFormulas() {
    formulas_.clear();
    unobsPreds_.clear();
    // TODO: nothing to do for default model, right?
}

inline void Domain::addFormula(const ELSentence& e) {
    ELSentence toAdd = e;
    if (toAdd.isQuantified()) {
        SISet set = toAdd.quantification();
        Interval spans = set.spanOf();
        if (spans.start() < maxInterval_.start()
                || spans.finish() > maxInterval_.finish()) {
            // need to resize
            Interval maxInt(std::min(spans.start(), maxInterval_.start()),
                    std::max(spans.finish(), maxInterval_.finish()));
            setMaxInterval(maxInt);
        }
    }
    // update our list of unobs preds
    PredCollector collect;
    toAdd.sentence()->visit(collect);
    for (std::set<Atom, atomcmp>::const_iterator it = collect.preds.begin(); it != collect.preds.end(); it++) {
        if (obsPreds_.count(*it) == 0) unobsPreds_.insert(*it);
    }
    formulas_.push_back(e);
}

inline const std::map<std::string, SISet>& Domain::observedPredicates() const {return obsPredsFixedAt_;};
inline NameGenerator& Domain::nameGenerator() {return generator_;};
inline Model Domain::defaultModel() const {return observations_;};

inline Interval Domain::maxInterval() const {return maxInterval_;};
inline SpanInterval Domain::maxSpanInterval() const {
    return SpanInterval(maxInterval_.start(), maxInterval_.finish(),
            maxInterval_.start(), maxInterval_.finish());
};
inline SISet Domain::maxSISet() const { return SISet(maxSpanInterval(), false);}

inline bool Domain::dontModifyObsPreds() const {return dontModifyObsPreds_;};
inline bool Domain::assumeClosedWorld() const {return assumeClosedWorld_;};
inline void Domain::setDontModifyObsPreds(bool b) {dontModifyObsPreds_ = b;};
inline void Domain::setAssumeClosedWorld(bool b) {assumeClosedWorld_ = b;};


#endif /* DOMAIN_H_ */
