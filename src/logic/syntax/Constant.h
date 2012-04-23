#ifndef CONSTANT_H
#define CONSTANT_H

#include <string>
#include <boost/functional/hash.hpp>
#include "term.h"

class Constant : public Term {
public:
    Constant(std::string name);
    Constant(const Constant& c);
    ~Constant();

    Constant& operator=(const Constant& other);

    friend std::size_t hash_value(const Constant& c);
protected:
    virtual void doToString(std::string& str) const;
private:
    std::string name_;

    virtual Term* doClone() const;
    virtual std::string doName() const;
    virtual bool doEquals(const Term& t) const;
    virtual std::size_t doHash() const;
};


// implementation
inline Constant::Constant(std::string name) : name_(name) {}
inline Constant::Constant(const Constant& c) : name_(c.name_) {}
inline Constant::~Constant() {}

inline Constant& Constant::operator=(const Constant& other) { if (this != &other) name_ = other.name_; return *this;}

// protected members
inline void Constant::doToString(std::string& str) const {str += name_;}

// private members
inline Term* Constant::doClone() const {return new Constant(*this);}

// TODO: what the hell is doname???
inline std::string Constant::doName() const {return name_;}
inline bool Constant::doEquals(const Term& t) const {
    // first try to cast t to a constant
    const Constant *con = dynamic_cast<const Constant*>(&t);
    if (con == NULL) {
        return false; // wrong type
    }
    return con->name_ == name_;
}

inline std::size_t Constant::doHash() const {return hash_value(*this);}

inline std::size_t hash_value(const Constant& c) {
    boost::hash<std::string> hasher;
    return hasher(c.name_);
}


#endif