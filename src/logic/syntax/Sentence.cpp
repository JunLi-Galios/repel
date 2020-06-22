#include "Sentence.h"
#include "Disjunction.h"
#include "Atom.h"
#include "BoolLit.h"
#include "Conjunction.h"
#include "DiamondOp.h"
#include "LiquidOp.h"
#include "Negation.h"
#include <sstream>
#include <boost/shared_ptr.hpp>

std::string TQConstraints::toString() const {
    std::stringstream str;

    if (mustBeIn.size() != 0) {
        str << "&" << mustBeIn.toString();
    }

    if (mustNotBeIn.size() != 0) {
        // only print a comma if we printed something before
        if (mustBeIn.size() != 0) {
            str << ",";
        }
        str << "\\" << mustNotBeIn.toString();
    }

    return str.str();
}

bool isDisjunctionOfPELCNFLiterals(const Sentence& s) {
    if (s.getTypeCode() != Disjunction::TypeCode) {
        LOG(LOG_WARN) << "sentence: \"" << s.toString() << "\". type is not Disjunction";
        return false;
    }
    const Disjunction& dis = static_cast<const Disjunction&>(s);
    
    if (!isPELCNFLiteral(*dis.left())){
        LOG(LOG_WARN) << "left sentence: \"" << dis.left()->toString() << "\". is not PELCNFLiteral";
    }
    if (!isDisjunctionOfPELCNFLiterals(*dis.left())){
        LOG(LOG_WARN) << "left sentence: \"" << dis.left()->toString() << "\". is not DisjunctionOfPELCNFLiterals";
    }
    if (!isPELCNFLiteral(*dis.right())){
        LOG(LOG_WARN) << "right sentence: \"" << dis.right()->toString() << "\". is not PELCNFLiteral";
    }
    if (!isDisjunctionOfPELCNFLiterals(*dis.right())){
        LOG(LOG_WARN) << "right sentence: \"" << dis.right()->toString() << "\". is not DisjunctionOfPELCNFLiterals";
    }

    if ((isPELCNFLiteral(*dis.left()) || isDisjunctionOfPELCNFLiterals(*dis.left()))
            && (isPELCNFLiteral(*dis.right()) || isDisjunctionOfPELCNFLiterals(*dis.right()))) {
        LOG(LOG_WARN) << "sentence: \"" << s.toString() << "\". is DisjunctionOfPELCNFLiterals";
        return true;
    }
    LOG(LOG_WARN) << "sentence: \"" << s.toString() << "\". is not DisjunctionOfPELCNFLiterals";
    return false;
}

bool isPELCNFLiteral(const Sentence& sentence) {
    if (sentence.getTypeCode() == Atom::TypeCode
            || sentence.getTypeCode() == BoolLit::TypeCode
            || sentence.getTypeCode() == LiquidOp::TypeCode) {
        return true;
    }
    if (sentence.getTypeCode() == Negation::TypeCode) {
        const Negation& neg = static_cast<const Negation&>(sentence);

        // TODO: necessary to check for double negation?
        if (neg.sentence()->getTypeCode() == Negation::TypeCode){
            const Negation& neg_neg = static_cast<const Negation&>(*neg.sentence());
            if (neg_neg.sentence()->getTypeCode() == Atom::TypeCode
            || neg_neg.sentence()->getTypeCode() == BoolLit::TypeCode
            || neg_neg.sentence()->getTypeCode() == LiquidOp::TypeCode
            || neg_neg.sentence()->getTypeCode() == DiamondOp::TypeCode){
                return true;
            }
            return false;
        } 
        return isPELCNFLiteral(*neg.sentence());
    }
    if (sentence.getTypeCode() == DiamondOp::TypeCode) {
        const DiamondOp& dia = static_cast<const DiamondOp&>(sentence);
        if (       dia.sentence()->getTypeCode()    == Atom::TypeCode
                || dia.sentence()->getTypeCode()    == BoolLit::TypeCode
                || dia.sentence()->getTypeCode()    == LiquidOp::TypeCode) {  // TODO add liquidop
            return true;
        }
        return false;
    }
    if (sentence.getTypeCode() == Conjunction::TypeCode) {
        const Conjunction& con = static_cast<const Conjunction&>(sentence);
        if ((con.left()->getTypeCode() == Atom::TypeCode
                || con.left()->getTypeCode() == BoolLit::TypeCode)
              && (con.right()->getTypeCode() == Atom::TypeCode
                || con.right()->getTypeCode() == BoolLit::TypeCode)) {
            return true;
        }
        return false;
    }

    return false;
}


bool isSimpleLiteral(const Sentence& lit) {
    if (lit.getTypeCode() == Atom::TypeCode) return true;
    if (lit.getTypeCode() == Negation::TypeCode) {
        const Negation& neg = static_cast<const Negation&>(lit);
        if (neg.sentence()->getTypeCode() == Atom::TypeCode) return true;
    }
    return false;
}
