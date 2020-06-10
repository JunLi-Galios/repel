#ifndef FOLPARSER_H
#define FOLPARSER_H

#include <vector>
#include <iterator>
#include <string>
#include <fstream>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include "FOLLexer.h"
#include "FOLToken.h"
#include "Domain.h"
#include "bad_parse.h"
#include "../SpanInterval.h"
#include "../Interval.h"
#include "ELSyntax.h"
#include "../Log.h"
#include "logic/syntax/Variable.h"

// anonymous namespace for helper functions
namespace {

struct find_max_interval : public std::unary_function<SpanInterval, void> {
    find_max_interval() : max() {}

    void operator()(const SpanInterval& i) {
        Interval x(i.start().start(), i.finish().finish());

        if (max.isNull()) {
            max = x;
        } else {
            max = span(max, x);
        }
    }

    Interval max;
};


template <class ForwardIterator>
struct iters {
    iters(ForwardIterator c, ForwardIterator l) : cur(c), last(l) {};
    ForwardIterator cur;
    ForwardIterator last;
};

class ParseOptions {
public:
    static const bool defAssumeClosedWorldInFacts = true;

    ParseOptions() : factsClosed_(defAssumeClosedWorldInFacts) {}

    bool assumeClosedWorldInFacts() const { return factsClosed_;}
    void setAssumeClosedWorldInFacts(bool b) {factsClosed_ = b;}

private:
    bool factsClosed_;
};

std::ostream& operator<<(std::ostream& o, const ParseOptions& p) {
    o << "{assumeClosedWorldInFacts = " << p.assumeClosedWorldInFacts() << "}";
    return o;
}

template <class ForwardIterator>
bool peekTokenType(FOLParse::TokenType type, iters<ForwardIterator> &its) {
    if (its.cur == its.last) return false;
    return its.cur->type() == type;
}

template <class ForwardIterator>
void consumeTokenType(FOLParse::TokenType type,
        iters<ForwardIterator> &its) throw (bad_parse) {
    if (its.cur == its.last) {
        bad_parse e;
        std::stringstream str;
        str << "unexpectedly reached end of tokens while parsing type " << type << std::endl;
        e.details = str.str();
        throw e;
    }
    if (its.cur->type() != type) {
        bad_parse e;
        std::stringstream str;

        str << "expected type " << type << " but instead we have: " << its.cur->type() << std::endl;
        str << "line number: " << its.cur->lineNumber() << ", column number: " << its.cur->colNumber() << std::endl;
        e.details = str.str();
        throw e;
    }
    its.cur++;
}

template <class ForwardIterator>
std::string consumeIdent(iters<ForwardIterator> &its) throw (bad_parse) {
    if (its.cur == its.last) {
        bad_parse e;
        std::stringstream str;
        str << "unexpectedly reached end of tokens while looking for identifier" << std::endl;
        e.details = str.str();
        throw e;
    }
    if (its.cur->type() != FOLParse::Identifier) {
        bad_parse e;
        std::stringstream str;
        str << "expected an identifier, instead we have type " << its.cur->type() << std::endl;
        str << "line number: " << its.cur->lineNumber() << ", column number: " << its.cur->colNumber() << std::endl;
        e.details = str.str();
        throw e;
    }
    std::string name = its.cur->contents();
    its.cur++;
    return name;
}

template <class ForwardIterator>
std::string consumeVariable(iters<ForwardIterator> &its) throw (bad_parse) {
    if (its.cur == its.last) {
        bad_parse e;
        std::stringstream str;
        str << "unexpectedly reached end of tokens while looking for variable" << std::endl;
        e.details = str.str();
        throw e;
    }
    if (its.cur->type() != FOLParse::Variable) {
        bad_parse e;
        std::stringstream str;
        str << "expected a variable, instead we have type " << its.cur->type() << std::endl;
        str << "line number: " << its.cur->lineNumber() << ", column number: " << its.cur->colNumber() << std::endl;
        e.details = str.str();
        throw e;
    }
    std::string name = its.cur->contents();
    its.cur++;
    return name;
}

template <class ForwardIterator>
unsigned int consumeNumber(iters<ForwardIterator> &its) throw (bad_parse) {
    if (its.cur == its.last) {
        bad_parse e;
        std::stringstream str;
        str << "unexpectedly reached end of tokens while looking for number" << std::endl;
        e.details = str.str();
        throw e;
    }
    if (its.cur->type() != FOLParse::Number) {
        bad_parse e;
        std::stringstream str;
        str << "expected a number, instead we have type " << its.cur->type() << std::endl;
        str << "line number: " << its.cur->lineNumber() << ", column number: " << its.cur->colNumber() << std::endl;
        e.details = str.str();
        throw e;
    }

    // use iostream to convert to int
    std::istringstream in(its.cur->contents());
    unsigned int num;
    in >> num;
    its.cur++;
    return num;
}

template <class ForwardIterator>
double consumeFloat(iters<ForwardIterator> &its) throw (bad_parse) {
    if (its.cur == its.last) {
        bad_parse e;
        std::stringstream str;
        str << "unexpectedly reached end of tokens while looking for number" << std::endl;
        e.details = str.str();
        throw e;
    }
    if (its.cur->type() != FOLParse::Float) {
        bad_parse e;
        std::stringstream str;
        str << "expected a float, instead we have type " << its.cur->type() << std::endl;
        str << "line number: " << its.cur->lineNumber() << ", column number: " << its.cur->colNumber() << std::endl;
        e.details = str.str();
        throw e;
    }

    // use iostream to convert to float
    std::istringstream in(its.cur->contents());
    double num;
    in >> num;
    its.cur++;
    return num;
}

template <class ForwardIterator>
bool endOfTokens(iters<ForwardIterator> &its) {
    return its.cur == its.last;
}


template <class ForwardIterator>
void doParseEvents(std::vector<FOL::Event>& store,
        std::map<std::string, std::set<std::string> >& objTypes,
        std::map<std::string, std::vector<std::string> >& predTypes,
        iters<ForwardIterator> &its) {
    while (!endOfTokens(its)) {
        if (peekTokenType(FOLParse::EndLine, its)) {
            consumeTokenType(FOLParse::EndLine, its);
        } else if (peekTokenType(FOLParse::Type, its)) {
            doParseType(objTypes, predTypes, its);
        } else { // must be an event
            std::vector<FOL::Event> events = doParseEvent(its);
            BOOST_FOREACH(FOL::Event event, events) {
                store.push_back(event);
            }
        }
    }
}

template <class ForwardIterator>
void doParseType(std::map<std::string, std::set<std::string> >& objTypes,
        std::map<std::string, std::vector<std::string> >& predTypes,
        iters<ForwardIterator> &its) {
    consumeTokenType(FOLParse::Type, its);
    consumeTokenType(FOLParse::Colon, its);
    std::string ident = consumeIdent(its);
    // if we have an equals, we are building a set of objs
    if (peekTokenType(FOLParse::Equals, its)) {
        std::set<std::string> objsInType;

        consumeTokenType(FOLParse::Equals, its);
        consumeTokenType(FOLParse::OpenBrace, its);
        if (peekTokenType(FOLParse::Identifier, its)) {
            objsInType.insert(consumeIdent(its));
            while (peekTokenType(FOLParse::Comma, its)) {
                consumeTokenType(FOLParse::Comma, its);
                objsInType.insert(consumeIdent(its));
            }
        }
        consumeTokenType(FOLParse::CloseBrace, its);

        // add our mapping to the obj types set, checking to see if it already
        // exists
        if (objTypes.count(ident) != 0) {
            LOG(LOG_WARN) << "already have an existing type definition for " << ident << " - using the new one.";
        }
        objTypes.insert(std::make_pair(ident, objsInType));
    } else {
        // must be a predicate type definition
        std::vector<std::string> predArgs;

        consumeTokenType(FOLParse::OpenParen, its);
        if (peekTokenType(FOLParse::Identifier, its)) {
            predArgs.push_back(consumeIdent(its));
            while (peekTokenType(FOLParse::Comma, its)) {
                consumeTokenType(FOLParse::Comma, its);
                predArgs.push_back(consumeIdent(its));
            }
        }
        consumeTokenType(FOLParse::CloseParen, its);

        if (predTypes.count(ident) != 0) {
            LOG(LOG_WARN) << "already have an existing type definition for predicate " << ident << " - using the new one";
        }
        predTypes.insert(std::make_pair(ident, predArgs));
    }
}


template <class ForwardIterator>
ELSentence doParseWeightedFormula(iters<ForwardIterator> &its) {
    bool hasWeight;
    double weight;
    if (peekTokenType(FOLParse::Number, its)) {
        hasWeight = true;
        weight = consumeNumber(its);
        consumeTokenType(FOLParse::Colon, its);
    } else if (peekTokenType(FOLParse::Float, its)) {
        hasWeight = true;
        weight = consumeFloat(its);
        consumeTokenType(FOLParse::Colon, its);

        //consumeTokenType(FOLParse::Float, its);
    } else {
        if (peekTokenType(FOLParse::Infinity, its)) {
            consumeTokenType(FOLParse::Infinity, its);
            consumeTokenType(FOLParse::Colon, its);
        }
        hasWeight = false;
        weight = 0.0;
    }
    boost::shared_ptr<Sentence> p = doParseFormula(its);
    ELSentence sentence(p);
    if (hasWeight) {
        sentence.setWeight(weight);
    } else {
        sentence.setHasInfWeight(true);
    }
    // check to see if it's quantified
    if (peekTokenType(FOLParse::At, its)) {
        consumeTokenType(FOLParse::At, its);
        //SISet set;
        std::vector<SpanInterval> sis;
        if (peekTokenType(FOLParse::OpenBrace, its)) {
            consumeTokenType(FOLParse::OpenBrace, its);
            while (!peekTokenType(FOLParse::CloseBrace, its)) {
                //SpanInterval si = doParseInterval(its);
                // normalize it
                boost::optional<SpanInterval> normSi = doParseInterval(its).normalize();
                if (normSi) sis.push_back(*normSi);
                if (!peekTokenType(FOLParse::CloseBrace, its)) {
                    consumeTokenType(FOLParse::Comma, its);
                }
            }
            consumeTokenType(FOLParse::CloseBrace, its);
        } else {
            boost::optional<SpanInterval> normSi = doParseInterval(its).normalize();
            if (normSi) sis.push_back(*normSi);
        }
        if (sis.empty()) {
            sentence.removeQuantification();
        } else {
            find_max_interval maxIntFinder;
            std::copy(sis.begin(), sis.end(), std::ostream_iterator<SpanInterval>(std::cout, ", "));
            maxIntFinder = std::for_each(sis.begin(), sis.end(), maxIntFinder);
            SISet set(false, maxIntFinder.max);
            for (std::vector<SpanInterval>::const_iterator it = sis.begin(); it != sis.end(); it++) {
                set.add(*it);
            }
            sentence.setQuantification(set);
        }
    } else {
        sentence.removeQuantification();
    }
    return sentence;
}
    
template <class ForwardIterator>
void doParseFormulas(std::vector<ELSentence>& store, std::map<std::string, std::set<std::string> >& objTypes,
        std::map<std::string, std::vector<std::string> >& predTypes, iters<ForwardIterator> &its) {
    while (!endOfTokens(its)) {
        if (peekTokenType(FOLParse::EndLine, its)) {
            consumeTokenType(FOLParse::EndLine, its);
        } else if (peekTokenType(FOLParse::Type, its)) {
            doParseType(objTypes, predTypes, its);
        } else {
            std::vector<std::vector<FOLToken> > tokenslist = parse_variable_tokens(objTypes, its);
            std::cout << "num of formulas = " << tokenslist.size() << std::endl;
            for (std::vector<std::vector<FOLToken> >::iterator it1 = tokenslist.begin(); it1 != tokenslist.end(); ++it1){
                iters<std::vector<FOLToken>::iterator> it2(it1->begin(), it1->end());                
                ELSentence formula = doParseWeightedFormula(it2);
                store.push_back(formula);
            }
        }
    }
}
    
template <class ForwardIterator>
std::vector<std::vector<FOLToken> > parse_variable_tokens(std::map<std::string, std::set<std::string> >& objTypes, iters<ForwardIterator> &its) {
    std::vector<std::vector<FOLToken> > tokenslist;
    std::map<Variable, std::vector<std::string> > predTypes;
    
    while (!endOfTokens(its)){
        std::cout << "current token = " << its.cur->contents() << std::endl;
        if (peekTokenType(FOLParse::EndLine, its)){
            consumeTokenType(FOLParse::EndLine, its);
            break;
        } else {
            if (tokenslist.size() == 0) {
                std::vector<FOLToken> tokens;
                tokenslist.push_back(tokens);
            }
            if (peekTokenType(FOLParse::Variable, its)){
                std::string var_name = consumeVariable(its);
                std::cout << "line 369 num of formulas = " << tokenslist.size() << std::endl;
                std::set<std::string> var_values = objTypes.find(var_name)->second;
                std::cout << "line 371 num of formulas = " << tokenslist.size() << std::endl;
                
                consumeTokenType(FOLParse::Colon, its);
                std::cout << "line 374 num of formulas = " << tokenslist.size() << std::endl;
                if (peekTokenType(FOLParse::Star, its)) {
                    consumeTokenType(FOLParse::Star, its);
                    std::cout << "line 377 num of formulas = " << tokenslist.size() << std::endl;
                    std::vector<std::vector<FOLToken> > tokenslist_copy;
                    for (std::vector<std::vector<FOLToken> >::iterator it1 = tokenslist.begin(); it1 != tokenslist.end(); ++it1){
                        for (std::set<std::string>::iterator it2 = var_values.begin(); it2 != var_values.end(); ++it2) {
                            std::cout << "line 381 num of formulas = " << tokenslist.size() << std::endl;
                            FOLToken token;
                            std::vector<FOLToken> tokens_copy(*it1);
                            std::string ident = *it2;
                            token.setType(FOLParse::Identifier);
                            std::cout << "line 385 num of formulas = " << tokenslist.size() << std::endl;
                            token.setContents(ident);
                            tokens_copy.push_back(token);
                            tokenslist_copy.push_back(tokens_copy);
                            std::cout << "line 390 num of formulas = " << tokenslist.size() << std::endl;
                        }                        
                    }                    
                    tokenslist = tokenslist_copy;
                    std::cout << "line 394 num of formulas = " << tokenslist.size() << std::endl;
                } else {
                    unsigned int num = consumeNumber(its);
                    Variable var_token(var_name, num);
                    std::cout << "line 398 num of formulas = " << tokenslist.size() << std::endl;
                    if (predTypes.find(var_token) != predTypes.end()) {
                        std::vector<std::string> idents = predTypes.find(var_token)->second;   
                        std::cout << "line 401 num of formulas = " << tokenslist.size() << std::endl;
                        std::vector<std::vector<FOLToken> >::iterator it1 = tokenslist.begin();
                        std::vector<std::string>::iterator  it3 = idents.begin(); 
                        for (;
                        it1 != tokenslist.end() || it3 != idents.end() ; 
                        ++it1, ++it3) {
                            std::cout << "line 407 num of formulas = " << tokenslist.size() << "num of idents = " << idents.size() << std::endl;
                            std::cout << "line 408 *it2 = " << *it3 << std::endl;
                            FOLToken token;
                            token.setType(FOLParse::Identifier);
                            token.setContents(*it3);
                            it1->push_back(token);
                        }
                        std::cout << "line 413 num of formulas = " << tokenslist.size() << std::endl;
                    } else {
                        std::vector<std::vector<FOLToken> > tokenslist_copy;
//                         std::map<Variable, std::vector<std::string> > predTypes_copy;
                        for (std::map<Variable, std::vector<std::string> >::iterator it4=predTypes.begin(); it4!=predTypes.end(); ++it4){
                            std::vector<std::string> str_vec;
                            for (std::vector<std::string>::iterator i5=it4->second.begin(); i5!=it4->second.end(); ++i5){
                                for (std::size_t i = 0; i < var_values.size(); ++i){
                                    str_vec.push_back(*i5);
                                }
                            }                            
                            predTypes[it4->first] = str_vec;
                        }
                        std::vector<std::string> pred_values;
                        for (std::vector<std::vector<FOLToken> >::iterator it1 = tokenslist.begin(); it1 != tokenslist.end(); ++it1){
                            for (std::set<std::string>::iterator it2 = var_values.begin(); it2 != var_values.end(); ++it2) {                                
                                FOLToken token;
                                std::vector<FOLToken> tokens_copy(*it1);
                                std::string ident = *it2;
                                pred_values.push_back(ident);
                                token.setType(FOLParse::Identifier);
                                token.setContents(ident);
                                tokens_copy.push_back(token);
                                tokenslist_copy.push_back(tokens_copy);                      
                            }                        
                        }
                        predTypes[var_token] = pred_values;
                        tokenslist = tokenslist_copy;
                        std::cout << "line 421 num of formulas = " << tokenslist.size() << std::endl;
                    }
                }                
            } else {
                std::cout << "line 425 num of formulas = " << tokenslist.size() << std::endl;
                for (std::vector<std::vector<FOLToken> >::iterator it1 = tokenslist.begin(); it1 != tokenslist.end(); ++it1){                    
                        it1->push_back(*(its.cur));
                }
                its.cur++;
            }
        }        
    }
    std::cout << "line 432 num of formulas = " << tokenslist.size() << std::endl;
    return tokenslist;
}

/*
template <class ForwardIterator>
void doParseInitFormulas(FormulaSet& store, iters<ForwardIterator> &its) {
    consumeTokenType(FOLParse::INIT, its);
    while (peekTokenType(FOLParse::EndLine, its)) {
        consumeTokenType(FOLParse::EndLine, its);
    }
    consumeTokenType(FOLParse::OpenBrace, its);
    while (!peekTokenType(FOLParse::CloseBrace, its)) {
        if (peekTokenType(FOLParse::EndLine, its)) {
            consumeTokenType(FOLParse::EndLine, its);
            continue;
        }
        WSentence formula = doParseWeightedFormula(its);
        store.addPrimaryFormula(formula);
    }
    consumeTokenType(FOLParse::CloseBrace, its);
}
*/

template <class ForwardIterator>
std::vector<FOL::Event> doParseEvent(iters<ForwardIterator> &its) {
    std::vector<FOL::Event> events;
    bool truthVal = true;
    if (peekTokenType(FOLParse::Not, its)) {
        consumeTokenType(FOLParse::Not, its);
        truthVal = false;
    }
    boost::shared_ptr<Atom> s = doParseGroundAtom(its);
    consumeTokenType(FOLParse::At, its);
    if (peekTokenType(FOLParse::OpenBrace, its)) {
        consumeTokenType(FOLParse::OpenBrace, its);
        if (peekTokenType(FOLParse::CloseBrace, its)) {
            consumeTokenType(FOLParse::CloseBrace, its);
            return events;
        }
        while (!peekTokenType(FOLParse::CloseBrace, its)) {
            FOL::Event event(s, doParseInterval(its), truthVal);
            events.push_back(event);
            if (!peekTokenType(FOLParse::CloseBrace, its)) consumeTokenType(FOLParse::Comma, its);
        }
        consumeTokenType(FOLParse::CloseBrace, its);

    } else {
        events.push_back(FOL::Event(s, doParseInterval(its), truthVal));
    }

    return events;
}

template <class ForwardIterator>
SpanInterval doParseInterval(iters<ForwardIterator> &its) {
    consumeTokenType(FOLParse::OpenBracket, its);
    return doParseInterval2(its);
}

template <class ForwardIterator>
SpanInterval doParseInterval2(iters<ForwardIterator> &its) {
    if (peekTokenType(FOLParse::Number, its)) {
        unsigned int i = consumeNumber(its);
        return doParseInterval3(i, its);
    } else {
        consumeTokenType(FOLParse::OpenParen, its);
        unsigned int i = consumeNumber(its);
        consumeTokenType(FOLParse::Comma, its);
        unsigned int j = consumeNumber(its);
        consumeTokenType(FOLParse::CloseParen, its);
        consumeTokenType(FOLParse::Comma, its);
        consumeTokenType(FOLParse::OpenParen, its);
        unsigned int k = consumeNumber(its);
        consumeTokenType(FOLParse::Comma, its);
        unsigned int l = consumeNumber(its);
        consumeTokenType(FOLParse::CloseParen, its);
        consumeTokenType(FOLParse::CloseBracket, its);

        return SpanInterval(Interval(i,j), Interval(k,l));
    }
}

template <class ForwardIterator>
SpanInterval doParseInterval3(unsigned int i, iters<ForwardIterator> &its) {
    if (peekTokenType(FOLParse::Comma, its)) {
        consumeTokenType(FOLParse::Comma, its);
        unsigned int k = consumeNumber(its );
        consumeTokenType(FOLParse::CloseBracket, its);
        return SpanInterval(Interval(i,i), Interval(k,k));
    } else {
        consumeTokenType(FOLParse::Colon, its);
        unsigned int k = consumeNumber(its);
        consumeTokenType(FOLParse::CloseBracket, its);
        return SpanInterval(Interval(i,k), Interval(i,k));
    }
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseEventLiteral(iters<ForwardIterator> &its) {
    if (peekTokenType(FOLParse::Not, its)) {
        consumeTokenType(FOLParse::Not, its);
        boost::shared_ptr<Atom> atom = doParseGroundAtom(its);
        boost::shared_ptr<Sentence> s(new Negation(atom));
        return s;
    }
    return doParseGroundAtom(its);
}

template <class ForwardIterator>
boost::shared_ptr<Atom> doParseGroundAtom(iters<ForwardIterator> &its) {
    std::string predName = consumeIdent(its);
    boost::shared_ptr<Atom> a(new Atom(predName));

    consumeTokenType(FOLParse::OpenParen, its);
    if (peekTokenType(FOLParse::CloseParen, its)) {
        // ok, empty atom, finish parsing
        consumeTokenType(FOLParse::CloseParen, its);
        return a;
    }
    std::auto_ptr<Term> c(new Constant(consumeIdent(its)));
    a->push_back(c); // ownership transfered to atom
    while (peekTokenType(FOLParse::Comma, its)) {
        consumeTokenType(FOLParse::Comma, its);
        std::auto_ptr<Term> cnext(new Constant(consumeIdent(its)));
        a->push_back(cnext);
    }
    consumeTokenType(FOLParse::CloseParen, its);

    return a;
}



template <class ForwardIterator>
boost::shared_ptr<Atom> doParseAtom(iters<ForwardIterator> &its) {
    std::string predName = consumeIdent(its);
    boost::shared_ptr<Atom> a(new Atom(predName));

    consumeTokenType(FOLParse::OpenParen, its);
    if (peekTokenType(FOLParse::Identifier, its)) {
        std::auto_ptr<Term> c(new Constant(consumeIdent(its)));
        a->push_back(c);
    } else if (peekTokenType(FOLParse::Variable, its)){
        std::auto_ptr<Term> v(new Variable(consumeVariable(its)));
        a->push_back(v);
    } else {
        // atom with no args
        consumeTokenType(FOLParse::CloseParen, its);
        return a;
    }
    while (peekTokenType(FOLParse::Comma, its)) {
        consumeTokenType(FOLParse::Comma, its);
        if (peekTokenType(FOLParse::Identifier, its)) {
            std::auto_ptr<Term> c(new Constant(consumeIdent(its)));
            a->push_back(c);
        } else {
            std::auto_ptr<Term> v(new Variable(consumeVariable(its)));
            a->push_back(v);
        }
    }
    consumeTokenType(FOLParse::CloseParen, its);
    return a;
}


template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseFormula(iters<ForwardIterator> &its) {
    return doParseFormula_exat(its);
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseFormula_exat(iters<ForwardIterator> &its) {  // TODO: support exactly 1/at most 1
    return doParseFormula_quant(its);
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseFormula_quant(iters<ForwardIterator> &its) { // TODO: support quantification
    return doParseFormula_imp(its);
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseFormula_imp(iters<ForwardIterator> &its) {
    boost::shared_ptr<Sentence> s = doParseFormula_or(its);
    while (peekTokenType(FOLParse::Implies, its)) {
        consumeTokenType(FOLParse::Implies, its);
        boost::shared_ptr<Sentence> s2 = doParseFormula_or(its);
        boost::shared_ptr<Negation> neg(new Negation(s));
        boost::shared_ptr<Sentence> dis(new Disjunction(neg, s2));
        s = dis;
    }
    return s;
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseFormula_or(iters<ForwardIterator> &its) {
    boost::shared_ptr<Sentence> s = doParseFormula_and(its);
    while (peekTokenType(FOLParse::Or, its)) {
        consumeTokenType(FOLParse::Or, its);
        boost::shared_ptr<Sentence> s2 = doParseFormula_and(its);
        boost::shared_ptr<Sentence> dis(new Disjunction(s,s2));
        s = dis;
    }
    return s;
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseFormula_and(iters<ForwardIterator> &its) {
    boost::shared_ptr<Sentence> s = doParseFormula_unary(its);
    while (peekTokenType(FOLParse::And, its) || peekTokenType(FOLParse::Semicolon, its)) {
        std::set<Interval::INTERVAL_RELATION> rels;
        if (peekTokenType(FOLParse::Semicolon, its)) {
            consumeTokenType(FOLParse::Semicolon, its);
            rels.insert(Interval::MEETS);   // meets is the default in this case;
        } else {
            consumeTokenType(FOLParse::And, its);
            rels = doParseRelationList(its);
            if (rels.empty()) {
                rels = Conjunction::defaultRelations();
            }
        }
        boost::shared_ptr<Sentence> s2 = doParseFormula_unary(its);
        boost::shared_ptr<Sentence> con(new Conjunction(s,s2,rels.begin(), rels.end()));
        s = con;
    }
    return s;
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseFormula_unary(iters<ForwardIterator> &its) {
    if (peekTokenType(FOLParse::Not, its)) {
        consumeTokenType(FOLParse::Not, its);
        boost::shared_ptr<Sentence> s = doParseFormula_unary(its);
        boost::shared_ptr<Sentence> neg(new Negation(s));
        return neg;
    } else if (peekTokenType(FOLParse::Diamond, its)) {
        consumeTokenType(FOLParse::Diamond, its);
        std::set<Interval::INTERVAL_RELATION> relations = doParseRelationList(its);
        if (relations.empty()) {
            // use default
            relations = DiamondOp::defaultRelations();
        }
        //boost::shared_ptr<Sentence> s = doParseFormula(its);
        boost::shared_ptr<Sentence> s = doParseFormula_unary(its);

        boost::shared_ptr<Sentence> dia(new DiamondOp(s, relations.begin(), relations.end()));
        return dia;
    } else {
        return doParseFormula_paren(its);
    }
}

template <class ForwardIterator>
std::set<Interval::INTERVAL_RELATION> doParseRelationList(iters<ForwardIterator> &its) {
    std::set<Interval::INTERVAL_RELATION> relations;
    if (peekTokenType(FOLParse::OpenBrace, its)) {
        consumeTokenType(FOLParse::OpenBrace, its);
        if (peekTokenType(FOLParse::Star, its)) {
            // add all relations
            relations.insert(Interval::MEETS);
            relations.insert(Interval::MEETSI);
            relations.insert(Interval::UMEETS);
            relations.insert(Interval::UMEETSI);
            relations.insert(Interval::OVERLAPS);
            relations.insert(Interval::OVERLAPSI);
            relations.insert(Interval::STARTS);
            relations.insert(Interval::STARTSI);
            relations.insert(Interval::DURING);
            relations.insert(Interval::DURINGI);
            relations.insert(Interval::FINISHES);
            relations.insert(Interval::FINISHESI);
            relations.insert(Interval::EQUALS);
            relations.insert(Interval::GREATERTHAN);
            relations.insert(Interval::LESSTHAN);
            consumeTokenType(FOLParse::Star, its);
        } else {
            relations.insert(doParseRelation(its));
        }
        while (peekTokenType(FOLParse::Comma, its)) {
            consumeTokenType(FOLParse::Comma, its);
            relations.insert(doParseRelation(its));
        }
        consumeTokenType(FOLParse::CloseBrace, its);
    }
    return relations;
}

template <class ForwardIterator>
Interval::INTERVAL_RELATION doParseRelation(iters<ForwardIterator>& its) {
    if (peekTokenType(FOLParse::Equals, its)) {
        consumeTokenType(FOLParse::Equals, its);
        return Interval::EQUALS;
    } else if (peekTokenType(FOLParse::GreaterThan, its)) {
        consumeTokenType(FOLParse::GreaterThan, its);
        return Interval::GREATERTHAN;
    } else if (peekTokenType(FOLParse::LessThan, its)) {
        consumeTokenType(FOLParse::LessThan, its);
        return Interval::LESSTHAN;
    } else {
        // looking for an ident here that matches
        std::string str = consumeIdent(its);
        if (str == "m") {
            return Interval::MEETS;
        } else if (str == "mi") {
            return Interval::MEETSI;
        } else if (str == "s") {
            return Interval::STARTS;
        } else if (str == "si") {
            return Interval::STARTSI;
        } else if (str == "d") {
            return Interval::DURING;
        } else if (str == "di") {
            return Interval::DURINGI;
        } else if (str == "f") {
            return Interval::FINISHES;
        } else if (str == "fi") {
            return Interval::FINISHESI;
        } else if (str == "o") {
            return Interval::OVERLAPS;
        } else if (str == "oi") {
            return Interval::OVERLAPSI;
        } else if (str == "um") {
            return Interval::UMEETS;
        } else if (str == "umi") {
            return Interval::UMEETSI;
        } else {
            // no more matches!
            bad_parse p;
            std::stringstream str;
            str << "no interval relation matches \"" << str << "\"" << std::endl;
            str << "line number: " << its.cur->lineNumber() << ", column number: " << its.cur->colNumber() << std::endl;
            p.details = str.str();
            throw p;
        }
    }
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseFormula_paren(iters<ForwardIterator> &its) {
    if (peekTokenType(FOLParse::OpenBracket, its)) {
        consumeTokenType(FOLParse::OpenBracket, its);
        boost::shared_ptr<Sentence> s = doParseStaticFormula(its);
        consumeTokenType(FOLParse::CloseBracket, its);

        boost::shared_ptr<Sentence> liq(new LiquidOp(s));
        return liq;
    } else if (peekTokenType(FOLParse::OpenParen, its)) {
        consumeTokenType(FOLParse::OpenParen, its);
        boost::shared_ptr<Sentence> s = doParseFormula(its);
        consumeTokenType(FOLParse::CloseParen, its);
        return s;
    } else if (peekTokenType(FOLParse::True, its)) {
        consumeTokenType(FOLParse::True, its);
        boost::shared_ptr<Sentence> s(new BoolLit(true));
        return s;
    } else if (peekTokenType(FOLParse::False, its)) {
        consumeTokenType(FOLParse::False, its);
        boost::shared_ptr<Sentence> s(new BoolLit(false));
        return s;
    } else {
        return doParseAtom(its);
    }
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseStaticFormula(iters<ForwardIterator> &its) {
    return doParseStaticFormula_exat(its);
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseStaticFormula_exat(iters<ForwardIterator> &its) {    // TODO: support exactly 1/at most 1
    return doParseStaticFormula_quant(its);
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseStaticFormula_quant(iters<ForwardIterator> &its) {   // TODO: support quantification
    return doParseStaticFormula_imp(its);
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseStaticFormula_imp(iters<ForwardIterator> &its) { // TODO: support double-implication
    boost::shared_ptr<Sentence> s1 = doParseStaticFormula_or(its);
    while (peekTokenType(FOLParse::Implies, its)) { // Implication is just special case of disjunction, ie X -> Y = !X v Y
        consumeTokenType(FOLParse::Implies, its);
        boost::shared_ptr<Sentence> s2 = doParseStaticFormula_or(its);
        boost::shared_ptr<Negation> neg(new Negation(s1));
        boost::shared_ptr<Sentence> dis(new Disjunction(neg, s2));

        s1 = dis;
    }
    return s1;
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseStaticFormula_or(iters<ForwardIterator> &its) {
    boost::shared_ptr<Sentence> s1 = doParseStaticFormula_and(its);
    while (peekTokenType(FOLParse::Or, its)) {
        consumeTokenType(FOLParse::Or, its);
        boost::shared_ptr<Sentence> s2 = doParseStaticFormula_and(its);
        boost::shared_ptr<Sentence> dis(new Disjunction(s1, s2));

        s1 = dis;
    }
    return s1;
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseStaticFormula_and(iters<ForwardIterator> &its) {
    boost::shared_ptr<Sentence> s1 = doParseStaticFormula_unary(its);
    while (peekTokenType(FOLParse::And, its)) {
            consumeTokenType(FOLParse::And, its);
            boost::shared_ptr<Sentence> s2 = doParseStaticFormula_unary(its);
            boost::shared_ptr<Sentence> con(new Conjunction(s1, s2));

            s1 = con;
    }
    return s1;
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseStaticFormula_unary(iters<ForwardIterator> &its) {
    if (peekTokenType(FOLParse::Not, its)) {
        consumeTokenType(FOLParse::Not, its);
        boost::shared_ptr<Sentence> s(new Negation(doParseStaticFormula_paren(its)));
        return s;
    } else {
        return doParseStaticFormula_paren(its);
    }
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> doParseStaticFormula_paren(iters<ForwardIterator> &its) {
    boost::shared_ptr<Sentence> s;
    if (peekTokenType(FOLParse::OpenParen, its)) {
        consumeTokenType(FOLParse::OpenParen, its);
        s = doParseStaticFormula(its);
        consumeTokenType(FOLParse::CloseParen, its);
    } else if (peekTokenType(FOLParse::True, its)) {
        consumeTokenType(FOLParse::True, its);
        boost::shared_ptr<Sentence> s(new BoolLit(true));
        return s;
    } else if (peekTokenType(FOLParse::False, its)) {
        consumeTokenType(FOLParse::False, its);
        boost::shared_ptr<Sentence> s(new BoolLit(false));
        return s;
    } else {
        s = doParseAtom(its);
    }
    return s;
}

};

namespace FOLParse 
{

// TODO: change this from parsing FOL::Events to Proposition/SISet pairs
void parseEventFile(const std::string &filename,
        std::vector<FOL::Event>& store,
        std::map<std::string, std::set<std::string> >& objTypes,
        std::map<std::string, std::vector<std::string> >& predTypes) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::runtime_error e("unable to open event file " + filename + " for parsing");
        throw e;
    }
    std::vector<FOLToken> tokens = FOLParse::tokenize(file);
//     BOOST_FOREACH(FOLToken token, tokens) {
//       std::cout << "type: " << token.type() <<  " contents: " << token.contents() << std::endl;
//     }

    iters<std::vector<FOLToken>::const_iterator > its(tokens.begin(), tokens.end());
    try {
        doParseEvents(store, objTypes, predTypes, its);
    } catch (bad_parse& e) {
        e.details += "filename: " +filename + "\n";
        file.close();
        throw;
    }
    file.close();
};

void parseFormulaFile(const std::string &filename,
        std::vector<ELSentence>& store,
        std::map<std::string, std::set<std::string> >& objTypes,
        std::map<std::string, std::vector<std::string> >& predTypes) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::runtime_error e("unable to open event file " + filename + " for parsing");
        throw e;
    }
    std::vector<FOLToken> tokens = FOLParse::tokenize(file);
    iters<std::vector<FOLToken>::const_iterator> its(tokens.begin(), tokens.end());
    try {
        doParseFormulas(store, objTypes, predTypes, its);
    } catch (bad_parse& e) {
        e.details += "filename: " + filename + "\n";
        file.close();
        throw;
    }
    file.close();
};

template <class ForwardIterator>
void parseFormulas(const ForwardIterator &first,
        const ForwardIterator &last, std::vector<ELSentence>& store) {
    iters<std::vector<FOLToken>::const_iterator> its(first, last);
//     doParseFormulas(store, its);
}

Domain loadDomainFromFiles(const std::string &eventfile, const std::string &formulafile, const ParseOptions& options=ParseOptions()) {
    std::vector<FOL::Event> events;
    //std::vector<WSentence> formulas;
    std::vector<ELSentence> formSet;

    std::map<std::string, std::set<std::string> > objTypes;
    std::map<std::string, std::vector<std::string> > predTypes;

    parseEventFile(eventfile, events, objTypes, predTypes);
    std::cout << "Read " << events.size() << " events from file." << std::endl;
    parseFormulaFile(formulafile, formSet, objTypes, predTypes);
    std::cout << "Read " << formSet.size() << " formulas from file." << std::endl;
    
    for(std::map<std::string, std::set<std::string> >::const_iterator it = objTypes.begin();
        it != objTypes.end(); ++it) {
            std::cout << "Define type: " << it->first << "=";
        for (std::set<std::string>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2){
            std::cout << " " << *it2;
        }
        std::cout << std::endl;
    }

    Domain d;
    boost::unordered_set<Atom> factAtoms;   // collect the fact atoms
    for (std::vector<FOL::Event>::const_iterator it = events.begin(); it != events.end(); it++) {
        // convert to proposition
        Proposition prop(*it->atom(), it->truthVal());
        factAtoms.insert(*it->atom());
        Interval maxInt(it->where().start().start(), it->where().finish().finish());
        SISet where(it->where(), true, maxInt);     // Locking all facts from the events file as liquid - need a better way to do this
        // TODO: type system!
        d.addFact(prop, where);
    }
    d.addFormulas(formSet.begin(), formSet.end());

    if (options.assumeClosedWorldInFacts()) {
        LOG(LOG_DEBUG) << "assuming closed world - adding in missing facts";
        // for every fact atom, subtract the places where it's not true, and add in the false statements
        for (boost::unordered_set<Atom>::const_iterator it = factAtoms.begin(); it != factAtoms.end(); it++) {
            SISet noFixedValue = d.getModifiableSISet(*it);
            if (!noFixedValue.empty()) {
                Proposition prop(*it, false);
                d.addFact(prop, noFixedValue);
            }
        }
    }
    return d;

}

template <class ForwardIterator>
void parseEvents(const ForwardIterator &first,
        const ForwardIterator &last,
        std::vector<FOL::Event>& store,
        std::map<std::string, std::set<std::string> >& objTypes,
        std::map<std::string, std::vector<std::string> >& predTypes) {
    iters<ForwardIterator> its(first, last);
    doParseEvents(store, objTypes, predTypes, its);
}

template <class ForwardIterator>
std::vector<FOL::Event> parseEvent(const ForwardIterator &first,
        const ForwardIterator &last) {
    iters<ForwardIterator> its(first, last);
    return doParseEvent(its);
}

template <class ForwardIterator>
SpanInterval parseInterval(const ForwardIterator &first,
        const ForwardIterator &last) {
    iters<ForwardIterator> its(first, last);
    return doParseInterval(its);
};

template <class ForwardIterator>
boost::shared_ptr<Atom> parseGroundAtom(const ForwardIterator &first,
        const ForwardIterator &last) {
    iters<ForwardIterator> its(first, last);
    return doParseGroundAtom(its);
};
template <class ForwardIterator>
boost::shared_ptr<Atom> parseAtom(const ForwardIterator &first,
        const ForwardIterator &last) {
    iters<ForwardIterator> its(first, last);
    return doParseAtom(its);
};

template <class ForwardIterator>
boost::shared_ptr<Sentence> parseFormula(const ForwardIterator &first,
        const ForwardIterator &last) {
    iters<ForwardIterator> its(first, last);
    return doParseFormula(its);
}

template <class ForwardIterator>
boost::shared_ptr<Sentence> parseStaticFormula(const ForwardIterator &first,
        const ForwardIterator &last) {
    iters<ForwardIterator> its(first, last);
    return doParseStaticFormula(its);
}

template <class ForwardIterator>
ELSentence parseWeightedFormula(const ForwardIterator &first,
        const ForwardIterator &last) {
    iters<ForwardIterator> its(first, last);
    return doParseWeightedFormula(its);
}

template <class ForwardIterator>
void parseTypes(const ForwardIterator &first,
        const ForwardIterator& last,
        std::map<std::string, std::set<std::string> >& objTypes,
        std::map<std::string, std::vector<std::string> >& predTypes) {
    iters<ForwardIterator> its(first, last);
    doParseType(objTypes, predTypes, its);
}
};


#endif
