//============================================================================
// Name        : pel.cpp
// Author      : Joe Selman
// Version     :

//============================================================================

#include "PELConfig.h"

#include <boost/program_options.hpp>
#include <boost/tuple/tuple.hpp>
namespace po = boost::program_options;
#include <boost/foreach.hpp>
#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>
#include <ctime>
#include <map>
#include <cstdlib>
#include "pel.h"
#include "logic/el_syntax.h"
#include "logic/folparser.h"
#include "logic/domain.h"
#include "log.h"
#include "logic/moves.h"
#include "inference/maxwalksat.h"
#include "logic/unit_prop.h"


int main(int argc, char* argv[]) {
    try {
        // Declare the supported options.
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("version,v", "print version and exit")
            ("max", po::value<unsigned int>(), "maximum value an interval endpoint can take")
            ("min", po::value<unsigned int>(), "minimum value an interval endpoint can take")
            ("evalModel,e", "simply print the model weight of the facts file")
            ("prob,p", po::value<double>()->default_value(0.25), "probability of taking a random move")
            ("iterations,i", po::value<unsigned int>()->default_value(1000), "number of iterations before returning a model")
            ("output,o", po::value<std::string>(), "output model file")
            ("unitProp,u", "perform unit propagation only and exit")
            ("datafile,d", po::value<std::string>(), "log scores from maxwalksat to this file (csv form)")
        ;

        po::options_description hidden("Hidden options");
        hidden.add_options()
            ("facts-file", po::value<std::string>(), "facts file")
            ("formula-file", po::value<std::string>(), "formula file")
        ;
        po::positional_options_description p;
        p.add("facts-file", 1);
        p.add("formula-file", 1);

        po::options_description cmdline_options;
        cmdline_options.add(desc).add(hidden);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("version")) {
            std::cout << "pel version " << PEL_VERSION_MAJOR << "." << PEL_VERSION_MINOR << std::endl;
            return EXIT_SUCCESS;
        }

        if (vm.count("help") || !vm.count("facts-file") || !vm.count("formula-file")) {
            std::cout << "Usage: pel [OPTION]... FACT-FILE FORMULA-FILE" << std::endl;
            std::cout << desc << std::endl;
            return EXIT_FAILURE;
        }

        // setup our logging facilities
        FILE* debugFile = fopen("debug.log", "w");
        if (!debugFile) std::cerr << "unable to open debug.log for logging - logging to stderr";
        else FilePolicy::stream() = debugFile;
        FileLog::globalLogLevel() = LOG_DEBUG;

        LOG(LOG_INFO) << "Opened log file for new session";

        // make sure we can open the output model file, if specified
        FILE* outputFile = NULL;
        if (vm.count("output")) {
            outputFile = fopen(vm["output"].as<std::string>().c_str(), "w");
            if (!outputFile) {
                std::cerr << "unable to open output file \"" << vm["output"].as<std::string>() << "\" for writing." << std::endl;
                return EXIT_FAILURE;
            }
        }

        Domain d = FOLParse::loadDomainFromFiles(vm["facts-file"].as<std::string>(), vm["formula-file"].as<std::string>());
        if (vm.count("max") || vm.count("min")) {
            Interval maxInt = d.maxInterval();
            if (vm.count("max")) maxInt.setFinish(vm["max"].as<unsigned int>());
            if (vm.count("min")) maxInt.setStart(vm["min"].as<unsigned int>());
            d.setMaxInterval(maxInt);
        }

        Model model = d.defaultModel();

        LOG_PRINT(LOG_INFO) << "model size: " << model.size();


        if (vm.count("evalModel")) {
            LOG(LOG_INFO) << "evaluating model...";
            unsigned long sum = 0;
            // evaluate the weight of each formula in the domain
            for(Domain::formula_const_iterator it = d.formulas_begin(); it != d.formulas_end(); it++) {
                ELSentence formula = *it;
                //SISet satisfied = d->satisfied(*(formula.sentence()), model);
                SISet satisfied = formula.sentence()->dSatisfied(model, d);
                unsigned long weight = d.score(formula, model);
                sum += weight;
                LOG_PRINT(LOG_INFO) << "formula: (" << formula.sentence()->toString() << ")";
                LOG_PRINT(LOG_INFO) << "\tsatisfied @ " << satisfied.toString();
                LOG_PRINT(LOG_INFO) << "\tscore contributed: " << weight;
            }
            LOG_PRINT(LOG_INFO) << "total score of model: " << sum;
        } else {
            if (vm.count("unitProp")) {
                LOG_PRINT(LOG_INFO) << "running unit propagation...";
                QUnitsFormulasPair reducedforms = performUnitPropagation(d);
                // TODO: enforce the unit props better.  for now we are just adding
                // them to the formula set.
                Domain newD;

                std::stringstream newForms;
                newForms << "Unit Clauses:\n";
                for (QCNFLiteralList::iterator it = reducedforms.first.begin(); it != reducedforms.first.end(); it++) {
                    ELSentence newS = convertFromQCNFClause(*it);
                    newS.setHasInfWeight(true);
                    newForms << "\t" << newS << "\n";
                    newD.addFact(newS);
                }
                newForms << "formulas:\n";
                for (QCNFClauseList::const_iterator it = reducedforms.second.begin(); it != reducedforms.second.end(); it++) {
                    ELSentence newS = convertFromQCNFClause(*it);
                    // should have inf weight
                    newS.setHasInfWeight(true);
                    newForms << "\t" << newS << "\n";
                    newD.addFormula(newS);
                }
                LOG(LOG_INFO) << "unit prop completed.\n" << newForms.str();

                // copy in all the unweighted formulas from the original d
                for (Domain::formula_const_iterator it = d.formulas_begin(); it != d.formulas_end(); it++) {
                    if (!it->hasInfWeight()) newD.addFormula(*it);
                }
                // replace the old form
                d = newD;

            }
            double p = vm["prob"].as<double>();
            unsigned int iterations = vm["iterations"].as<unsigned int>();

            // rewrite all our infinite weighted sentences
            LOG(LOG_INFO) << "rewriting infinite weights as pseudo-weights using factor of " << Domain::hardFormulaFactor;
            d = d.replaceInfForms();
            LOG(LOG_INFO) << "searching for a maximum-weight model, with p=" << p << " and iterations=" << iterations;
            Model defModel = d.defaultModel();

            std::fstream datastream;
            if(vm.count("datafile")) {
                datastream.open(vm["datafile"].as<std::string>().c_str(), std::fstream::out);
                if (datastream.fail()) {
                    LOG_PRINT(LOG_ERROR) << "unable to open file \"" << vm["datafile"].as<std::string>() << "\" for data file storage.";
                }
            }
            Model maxModel(d.maxInterval());
            if (datastream.is_open() && datastream.good()) {
                maxModel = maxWalkSat(d, iterations, p, &defModel, &datastream);
            } else {
                maxModel = maxWalkSat(d, iterations, p, &defModel, 0);
            }

            if (datastream.is_open()) datastream.close();

            LOG_PRINT(LOG_INFO) << "Best model found: " << std::endl;
            LOG_PRINT(LOG_INFO) << maxModel.toString();
            if (vm.count("output")) {
                // log it to the output file as well
                fprintf(outputFile, "# generated from fact file \"%s\" and formula file \"%s\"\n",
                        vm["facts-file"].as<std::string>().c_str(),
                        vm["formula-file"].as<std::string>().c_str());
                std::string timeStr;
                {
                    time_t rawtime;
                    struct tm * timeinfo;
                    time (&rawtime);
                    timeinfo = localtime (&rawtime);
                    timeStr = asctime(timeinfo);
                }
                fprintf(outputFile, "# generated on %s\n", timeStr.c_str());
                fprintf(outputFile, "# run with %d iterations and %g chance of choosing a random move\n",
                         iterations,
                        p);
                fputs(maxModel.toString().c_str(), outputFile);
            }
        }

        // Should be good and close files?
        return EXIT_SUCCESS;
    } catch (std::exception& e) {
        std::cerr << "Exception : " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
        return EXIT_FAILURE;
    }
}
