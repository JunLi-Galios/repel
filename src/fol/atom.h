#ifndef ATOM_H
#define ATOM_H

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include "term.h"
#include "sentence.h"
#include "sentencevisitor.h"
#include "constant.h"

class Atom : public Sentence {
public:
	typedef std::vector<boost::shared_ptr<Term> >::size_type size_type;

	Atom(std::string name) : pred(name) {};
	Atom(std::string name, boost::shared_ptr<Term> term) : pred(name) {
		terms.push_back(term);
	}
	Atom(const Atom& a) : pred(a.pred), terms(a.terms) {};	// shallow copy
	template <typename ForwardIterator>
	Atom(std::string name,
			ForwardIterator first,
			ForwardIterator last) : pred(name) {
		ForwardIterator it = first;
		while (it != last) {
			boost::shared_ptr<Term> t(it->clone());
			terms.push_back(t);
			it++;
		}
	};

	bool isGrounded() const {
		for (std::vector<boost::shared_ptr<Term> >::const_iterator it = terms.begin(); it != terms.end(); it++) {

			boost::shared_ptr<Constant> constant = boost::dynamic_pointer_cast<Constant>(*it);
			if (constant.get() == 0) return false;
		}
		return true;
	};

	int arity() const {return terms.size();};
	std::string name() const {return pred;};
	std::string& name() {return pred;};

	Atom& operator=(const Atom& b) {							// TODO add this to all subclasses of sentence!
		pred = b.pred;
		terms = std::vector<boost::shared_ptr<Term> >(b.terms);
		return *this;
	}
	//Term& operator[] (boost::ptr_vector<Term>::size_type n) {return terms[n];};
	//const Term& operator[] (boost::ptr_vector<Term>::size_type n) const {return terms[n];};
	// TODO make the at() function throw an exception
	boost::shared_ptr<Term> at(size_type n) {return terms[n];};
	boost::shared_ptr<const Term> at(size_type n) const {return terms[n];};

	void push_back(const boost::shared_ptr<Term>& t)  {terms.push_back(t);};
	virtual void visit(SentenceVisitor& v) const {
		v.accept(*this);
	}
private:
	std::string pred;
	//boost::ptr_vector<Term> terms;
	std::vector<boost::shared_ptr<Term> > terms;

	virtual Sentence* doClone() const {return new Atom(*this);};	// TODO is shallow copy what we want here?

	virtual bool doEquals(const Sentence& t) const {
		const Atom *at = dynamic_cast<const Atom*>(&t);
		if (at == NULL) {
			return false;
		}

		return (pred == at->pred)
				&& std::equal(boost::make_indirect_iterator(terms.begin()),
						boost::make_indirect_iterator(terms.end()),
						boost::make_indirect_iterator(at->terms.begin()));
	};

	virtual void doToString(std::string& str) const {
		str += pred;
		str += "(";
		for (std::vector<boost::shared_ptr<Term> >::const_iterator it = terms.begin();
				it != terms.end();
				it++) {
			str += (*it)->toString();
			if (it + 1 != terms.end()) {
				str += ", ";
			}
		}
		str += ")";
	};

	virtual int doPrecedence() const {
		return 0;
	};
};

struct atomcmp {
	bool operator()(const Atom& a, const Atom& b) const {
		return a.toString() < b.toString();
	}
};

#endif
