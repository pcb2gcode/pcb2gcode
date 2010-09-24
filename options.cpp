
#include "options.hpp"
#include "config.h"

#include <fstream>
#include <list>
#include <boost/foreach.hpp>
#include <boost/exception.hpp>

options&
options::instance()
{
	static options singleton;
	return singleton;
}


void
options::parse_cl( int argc, char** argv )
{
	try { 
		po::options_description generic;
		generic.add(instance().cli_options).add(instance().cfg_options);
		po::store(po::parse_command_line(argc, argv, generic), instance().vm);
	}
	catch( boost::exception& e ) { 
		throw std::runtime_error("Invalid option.\n");
	}

	po::notify( instance().vm );

	parse_files();
}

string
options::help()
{
	std::stringstream msg;
	msg << PACKAGE_STRING << "\n\n";
	msg << instance().cli_options << instance().cfg_options;
	return msg.str();
}

#include <iostream>
using std::cerr;
using std::endl;

void
options::parse_files()
{
	std::string file("millproject");

	try {
		std::ifstream stream;
		try {
			stream.open(file.c_str());
			po::store( po::parse_config_file( stream, instance().cfg_options), instance().vm);
		} catch( std::exception& e ) {
			cerr << "Error parsing configuration file \"" << file << "\": "
			     << e.what() << endl;
		}
		stream.close();
	}
	catch( std::exception& e ) {
		cerr << "Error reading configuration file \"" << file << "\": " << e.what() << endl;
	}

	po::notify( instance().vm );
}

options::options() : cli_options("command line only options"),
		     cfg_options("generic options (CLI and config files)")
{
	cli_options.add_options()
		("help,?",   "produce help message")
		("version",  "\n")
		;

	cfg_options.add_options()
		("front",      po::value<string>(), "front side RS274-X .gbr")
		("back",   po::value<string>(), "back side RS274-X .gbr")
		("outline",  po::value<string>(), "pcb outline RS274-X .gbr; outline drawn in 10mil traces")
//		("margins",  po::value<double>(), "pcb margins in inches, substitute for --outline\n")
		("zwork",    po::value<double>(), "milling depth in inches (Z-coordinate while engraving)")
		("zsafe",      po::value<double>(), "safety height (Z-coordinate during rapid moves)")
		("offset",   po::value<double>(), "distance between the PCB traces and the end mill path in inches; usually half the isolation width")
		("mill-feed", po::value<double>(), "feed while isolating in ipm")
		("mill-speed", po::value<int>(), "spindle rpm when milling\n")
		("cutter-diameter", po::value<double>(), "diameter of the end mill used for cutting out the PCB\n")
		("zcut", po::value<double>(), "PCB cutting depth in inches.")
		("cut-feed", po::value<double>(), "PCB cutting feed")
		("cut-speed", po::value<int>(), "PCB cutting spindle speed")
		("cut-infeed", po::value<double>(), "Maximum cutting depth; PCB may be cut in multiple passes")
		("drill", po::value<string>(), "Excellon drill file")
		("zdrill", po::value<double>(), "drill depth")
		("zchange", po::value<double>(), "tool changing height")
		("drill-feed", po::value<double>(), "drill feed; ipm")
		("drill-speed", po::value<int>(), "spindle rpm when drilling\n")
		("dpi",      po::value<int>()->default_value(1000),   "virtual photoplot resolution")
		;
}


static void check_generic_parameters( po::variables_map const& vm )
{
	int dpi = vm["dpi"].as<int>();
	if( dpi < 100 ) cerr << "Warning: very low DPI value." << endl;
	if( dpi > 10000 ) cerr << "Warning: very high DPI value, processing may take extremely long" << endl;

	if( !vm.count("zsafe") ) {
		cerr << "Error: Safety height not specified.\n";
		exit(5);
	}
}

static void check_milling_parameters( po::variables_map const& vm )
{
	if( vm.count("front") || vm.count("back") ) {
		if( !vm.count("zwork") ) {
			cerr << "Error: --zwork not specified.\n";
			exit(1);
		} else if( vm["zwork"].as<double>() > 0 ) {
			cerr << "Warning: Engraving depth (--zwork) is greater than zero!\n";
		}
		
		if( !vm.count("offset") ) {
			cerr << "Error: Etching --offset not specified.\n";
			exit(4);
		}
	}
}

static void check_drilling_parameters( po::variables_map const& vm )
{
	if( vm.count("drill") ) {
		if( !vm.count("zdrill") ) {
			cerr << "Error: Drilling depth (--zdrill) not specified.\n";
			exit(9);
		}
		if( !vm.count("zchange") ) {
			cerr << "Error: Drill bit changing height (--zchange) not specified.\n";
			exit(10);
		}
		if( !vm.count("drill-feed") ) {
			cerr << "Error:: Drilling feed (--drill-feed) not specified.\n";
			exit(11);
		}
		if( !vm.count("drill-speed") ) {
			cerr << "Error: Drilling spindle RPM (--drill-speed) not specified.\n";
			exit(12);
		}
	}
}

static void check_cutting_parameters( po::variables_map const& vm )
{
	if( vm.count("outline") ) {
		if( !vm.count("zcut") ) {
			cerr << "Error: Board cutting depth (--zcut) not specified.\n";
			exit(5);
		}
		if( !vm.count("cut-feed") ) {
			cerr << "Error: Board cutting feed (--cut-feed) not specified.\n";
			exit(6);
		}
		if( !vm.count("cut-speed") ) {
			cerr << "Error: Board cutting spindle RPM (--cut-speed) not specified.\n";
			exit(7);
		}
		if( !vm.count("cut-infeed") ) {
			cerr << "Error: Board cutting infeed (--cut-infeed) not specified.\n";
			exit(8);
		}		
	}
}

void options::check_parameters()
{
	po::variables_map const& vm = instance().vm;
	check_generic_parameters( vm );
	check_milling_parameters( vm );
	check_cutting_parameters( vm );
}
