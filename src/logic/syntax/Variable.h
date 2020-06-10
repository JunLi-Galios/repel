#ifndef VARIABLE_H
#define VARIABLE_H

#include <string>
#include <boost/functional/hash.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/void_cast.hpp>
#include "Term.h"

class Variable : public Term {
public:
    Variable(std::string name="_Unknown"); 
    Variable(std::string name, std::size_t id) : name_(name), id_(id) {};
    Variable(const Variable& c) : name_(c.name_), id_(c.id_) {}; 
    ~Variable() {};

    void operator=(const Variable& c) {name_ = c.name_; id_ = c.id_;};

    friend std::size_t hash_value(const Variable& v);
protected:
    void doToString(std::string& str) const;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);

    virtual Term* doClone() const;
    virtual std::string doName() const {return name_;};
    virtual bool doEquals(const Term& t) const;
    virtual std::size_t doHash() const;
    std::string name_;
    std::size_t id_;
    bool operator==(const Variable& l, const Variable& r);
    bool operator!=(const Variable& l, const Variable& r);
};


inline std::size_t Variable::doHash() const {return hash_value(*this);}

// IMPLEMENTATION
inline std::size_t hash_value(const Variable& v) {
    boost::hash_combine(seed, v.name_);
    boost::hash_combine(seed, v.id_);
    return seed;
}

inline bool operator==(const Variable& lhs, const Variable& rhs) {return (lhs.name_ == rhs.name_ && lhs.id_ == rhs.id_);}
inline bool operator!=(const Variable& lhs, const Variable& rhs) {return !operator==(lhs, rhs);}

template <class Archive>
void Variable::serialize(Archive& ar, const unsigned int version) {
    // register that we dont need to serialize the base class
    boost::serialization::void_cast_register<Variable, Term>(
            static_cast<Variable*>(NULL),
            static_cast<Term*>(NULL)
            );
    ar & name_;
    ar & id_;
}

#endif
