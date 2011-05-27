//============================================================================
// Name        : pel.cpp
// Author      : Joe Selman
// Version     :

//============================================================================

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <boost/program_options.hpp>
namespace po = boost::program_options;
#include <boost/foreach.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include "fol/fol.h"
#include "fol/folparser.h"
#include "fol/domain.h"

int main(int argc, char* argv[]) {
	// Declare the supported options.
	po::options_description desc("Allowed options");
	desc.add_options()
	    ("help", "produce help message")
	;

	po::options_description hidden("Hidden options");
	hidden.add_options()
		("facts-file", po::value<std::string>(), "facts file")
		("formula-file", po::value<std::string>(), "formula file")
	;
	po::positional_options_description p;
	p.add("facts-file", 1);
	p.add("formula-file", 2);

	po::options_description cmdline_options;
	cmdline_options.add(desc).add(hidden);

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
	po::notify(vm);

	if (vm.count("help") || !vm.count("facts-file") || !vm.count("formula-file")) {
	    std::cout << "Usage: pel [OPTION]... FACT-FILE FORMULA-FILE" << std::endl;
		std::cout << desc << std::endl;
	    return 1;
	}

	boost::shared_ptr<Domain> d = FOLParse::loadDomainFromFiles(vm["facts-file"].as<std::string>(), vm["formula-file"].as<std::string>());
	Model observations = d->defaultModel();

	for (Model::const_iterator it = observations.begin(); it != observations.end(); it++) {
		Atom atom = it->first;
		SISet when = it->second;

		std::cout << "Atom: " << atom.toString() << " @ " << when.toString() << std::endl;
	}
	return 0;
}
