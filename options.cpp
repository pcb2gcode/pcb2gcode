
/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010, 2011 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others
 * 
 * pcb2gcode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pcb2gcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "options.hpp"
#include "config.h"

#include <fstream>
#include <list>
#include <boost/foreach.hpp>
#include <boost/exception/all.hpp>

#include <iostream>
using std::cerr;
using std::endl;


options&
options::instance()
{
	static options singleton;
	return singleton;
}

void
options::parse( int argc, char** argv )
{
	instance().sources.push_front( GetPot(argc, argv) );
}

void
options::parse( const string file )
{
	instance().sources.push_back( GetPot(file.c_str()) );
}


/* returns true if the flag "name" is set in ANY of the config files
 * or specified as a command line argument
 */
bool
options::flag( string name )
{
	list<GetPot>& sources = instance().sources;

	// check if --name was specified on the command line
	if( sources.begin()->search( (string("--")+name).c_str() ) ) return true;

	// check if it is in any of the config files
	for( list<GetPot>::iterator it = sources.begin(); it != sources.end(); it++ )
		if( it->search( name.c_str() ) ) return true;

	// --name not found
	return false;
}

/* same as flag(), bug checks only the command line.
 * this is useful for commands like --verbose, --help and --version
 */
bool
options::cl_flag( string name )
{
	list<GetPot>& sources = instance().sources;

	// check if --name was specified on the command line
	if( sources.begin()->search( (string("--")+name).c_str() ) )
		return true;
	else
		return false;
}


bool
options::have_dbl( string name )
{
	try {
		dbl( name );
		return true;
	} catch ( std::runtime_error ) {
		return false;
	}
}

/* get a parameter of type double
 *
 * - priorities of the config files in order of loading, the first loaded
 *   is of the highest priority
 * - command line parameters override everything
 * - if nothing is found, throw a runtime_error
 */
double
options::dbl( string name )
{
	list<GetPot>& sources = instance().sources;

	// check command line
	string dashname = string("--") + name;
	try {
		// try variable style: --param=123.4
		return (*sources.begin())(dashname.c_str(), (double)0 );
	} catch( std::runtime_error ) {
		try {
			// try "arguments that follow arguments"-style: --param 123.4
			return sources.begin()->follow( (double)0, dashname.c_str() );
		} catch( std::runtime_error ) {	}
	};

	// check config files
	for( list<GetPot>::iterator it = sources.begin(); it != sources.end(); it++ )
		try {
			return (*it)(name.c_str(), (double)0 );
		} catch( std::runtime_error ) {};


	std::stringstream msg; msg << "Parameter " << name << "(double) not found.";
	throw std::runtime_error( msg.str() );
}

double
options::dbl( string name, double default_value )
{
	try {
		return dbl(name);
	} catch( std::runtime_error ) {
		return default_value;
	}
}


bool
options::have_str( string name )
{
	try {
		str( name );
		return true;
	} catch ( std::runtime_error ) {
		return false;
	}
}

string
options::str( string name )
{
	list<GetPot>& sources = instance().sources;

	// check command line
	string dashname = string("--") + name;
	try {
		// try variable style
		return (*sources.begin())(dashname.c_str(), string(""));
	} catch( std::runtime_error ) {
		try {
			// try argument follow style
			return sources.begin()->follow( string("").c_str(), dashname.c_str() );
		} catch( std::runtime_error ) {}
	}

	// check config files
	for( list<GetPot>::iterator it = sources.begin(); it != sources.end(); it++ )
		try {
			return (*it)(name.c_str(), string(""));
		} catch( std::runtime_error ) { };


	std::stringstream msg; msg << "Parameter " << name << "(string) not found.";
	throw std::runtime_error( msg.str() );
}

string
options::str( string name, string default_value )
{
	try {
		return str(name);
	} catch( std::runtime_error ) {
		return default_value;
	}
}

string
options::help()
{
	std::stringstream msg;
	msg << PACKAGE_STRING << "\n\n";

	msg << "command line only options:\n";
	msg << "  --help, -?                 " << "produce help message" << endl;
	msg << "  --version                  " << "print version information" << endl << endl;

	msg << "generic options (CLI and config files):" << endl;
	msg << "  --front                    " << "front side RS274-X .gbr" << endl;
	msg << "  --back                     " << "back side RS274-X .gbr" << endl;
	msg << "  --outline                  " << "pcb outline polygon RS274-X .gbr" << endl;
	msg << "  --drill                    " << "Excellon drill file" << endl << endl;

	msg << "  --zwork                    " << "milling depth in inches (Z-coordinate while engraving)" << endl;
	msg << "  --zsafe                    " << "safety height (Z-coordinate during rapid moves)" << endl;
	msg << "  --offset                   " << "distance between the PCB traces and the end mill path in" << endl
	    << "                             " << "inches; usually half the isolation width" << endl;
	msg << "  --mill-feed                " << "feed while isolating in ipm" << endl;
	msg << "  --mill-speed               " << "spindle rpm when milling" << endl;
	msg << "  --milldrill                " << "drill using the mill head" << endl;

	msg << "  --extra-passes             " << "specify the the number of extra isolation passes, increasing" << endl
	    << "                             " << "the isolation width half the tool diameter with each pass" << endl;

	msg << "  --fill-outline             " << "accept a contour instead of a polygon as outline" << endl;
	msg << "  --outline-width            " << "width of the outline" << endl;
	msg << "  --cutter-diameter          " << "diameter of the end mill used for cutting out the PCB" << endl;
	msg << "  --zcut                     " << "PCB cutting depth in inches." << endl;
	msg << "  --cut-feed                 " << "PCB cutting feed" << endl;
	msg << "  --cut-speed                " << "PCB cutting spindle speed" << endl;
	msg << "  --cut-infeed               " << "Maximum cutting depth; PCB may be cut in multiple passes" << endl << endl;

	msg << "  --zdrill                   " << "drill depth" << endl;
	msg << "  --zchange                  " << "tool changing height" << endl;
	msg << "  --drill-feed               " << "drill feed; ipm" << endl;
	msg << "  --drill-speed              " << "spindle rpm when drilling" << endl;
	msg << "  --drill-front              " << "drill through the front side of board" << endl << endl;

	msg << "  --metric                   " << "use metric instead of imperial units" << endl;
	msg << "  --dpi                      " << "virtual photoplot resolution" << endl;
	msg << "  --mirror-absolute          " << "mirror back side along absolute zero instead of board center" << endl << endl;

	msg << "  --svg                      " << "SVG output file" << endl;
	msg << "  --basename                 " << "prefix for default output file names" << endl;
	msg << "  --front-output             " << "output file for front layer" << endl;
	msg << "  --back-output              " << "output file for back layer" << endl;
	msg << "  --outline-output           " << "output file for outline" << endl;
	msg << "  --drill-output             " << "output file for drilling" << endl << endl;
	
	msg << "  --preamble                 " << "gcode preamble file" << endl;
	msg << "  --postamble                " << "gcode postamble file" << endl;

	return msg.str();
}


void 
options::check_generic_parameters()
{
	int dpi = options::dbl("dpi");
	if( dpi < 100 ) cerr << "Warning: very low DPI value." << endl;
	if( dpi > 10000 ) cerr << "Warning: (possibly) unreasonably high DPI value, processing may take very long" << endl;

	if( !options::have_dbl("zsafe") ) {
		cerr << "Error: Safety height not specified.\n";
		exit(5);
	}
	if( !options::have_dbl("zchange") ) {
		cerr << "Error: Tool changing height not specified.\n";
		exit(15);
	}
}

void
options::check_milling_parameters()
{
	if(options::have_str("front") || options::have_str("back")) {
		if( !options::have_dbl("zwork") ) {
			cerr << "Error: --zwork not specified.\n";
			exit(1);
		} else if( options::dbl("zwork") > 0.0001 ) {
			cerr << "Warning: Engraving depth (--zwork) is greater than zero!\n";
		}

		if( !options::have_dbl("offset") ) {
			cerr << "Error: Etching --offset not specified.\n";
			exit(4);
		}
		if( !options::have_dbl("mill-feed") ) {
			cerr << "Error: Milling feed [ipm] not specified.\n";
			exit(13);
		}
		if( !options::have_dbl("mill-speed") ) {
			cerr << "Error: Milling speed [rpm] not specified.\n";
			exit(14);
		}
		
		// required parameters present. check for validity.
		if( options::dbl("zsafe") <= options::dbl("zwork") ) {
			cerr << "Error: The safety height --zsafe is lower than the milling "
			     << "height --zwork.\n";
			exit(15);
		}

		if( options::dbl("mill-feed") < 0 ) {
			cerr << "Error: Negative milling feed (--mill-feed).\n";
			exit(17);
		}

		if( options::dbl("mill-speed") < 0 ) {
			cerr << "Error: --mill-speed < 0.\n";
			exit(16);
		}
	}
}

void
options::check_drilling_parameters()
{
	if( options::have_str("drill") ) {
		if( !options::have_dbl("zdrill") ) {
			cerr << "Error: Drilling depth (--zdrill) not specified.\n";
			exit(9);
		}
		if( !options::have_dbl("zchange") ) {
			cerr << "Error: Drill bit changing height (--zchange) not specified.\n";
			exit(10);
		}
		if( !options::have_dbl("drill-feed") ) {
			cerr << "Error:: Drilling feed (--drill-feed) not specified.\n";
			exit(11);
		}
		if( !options::have_dbl("drill-speed") ) {
			cerr << "Error: Drilling spindle RPM (--drill-speed) not specified.\n";
			exit(12);
		}
		
		if( options::dbl("zsafe") <= options::dbl("zdrill") ) {
			cerr << "Error: The safety height --zsafe is lower than the drilling "
			     << "height --zdrill!\n";
			exit(18);
		}
		if( options::dbl("zchange") <= options::dbl("zdrill") ) {
			cerr << "Error: The safety height --zsafe is lower than the tool "
			     << "change height --zchange!\n";
			exit(19);
		}
		if( options::dbl("drill-feed") <= 0 ) {
			cerr << "Error: The drilling feed --drill-feed is <= 0.\n";
			exit(20);
		}
		if( options::dbl("drill-speed") < 0 ) {
			cerr << "Error: --drill-speed < 0.\n";
			exit(17);
		}
	}
}

void
options::check_cutting_parameters()
{
	if( options::have_str("outline") || (options::have_str("drill") && options::flag("milldrill"))) {
		if( options::flag("fill-outline") ) {
			if(!options::have_dbl("outline-width")) {
				cerr << "Error: For outline filling, a width (--outline-width) has to be specified.\n";
				exit(25);
			} else {
				double outline_width = options::dbl("outline-width");
				if( outline_width < 0 ) {
					cerr << "Error: Specified outline width is less than zero!\n";
					exit(26);
				} else if( outline_width == 0 ) {
					cerr << "Error. Specified outline width is zero!\n";
					exit(27);
				} else {
					std::stringstream width_sb;
					if( (options::flag("metric") && outline_width >= 10)
					    || (!options::flag("metric") && outline_width >= 0.4) ) {
						
						width_sb << outline_width << (options::flag("metric") ? " mm" : " inch");
						cerr << "Warning: You specified an outline-width of " << width_sb.str() << "!\n";
					}
				}
			}
		}

		if( !options::have_dbl("zcut") ) {
			cerr << "Error: Board cutting depth (--zcut) not specified.\n";
			exit(5);
		}
		if( !options::have_dbl("cutter-diameter") ) {
			cerr << "Error: Cutter diameter not specified.\n";
			exit(15);
		}
		if( !options::have_dbl("cut-feed") ) {
			cerr << "Error: Board cutting feed (--cut-feed) not specified.\n";
			exit(6);
		}
		if( !options::have_dbl("cut-speed") ) {
			cerr << "Error: Board cutting spindle RPM (--cut-speed) not specified.\n";
			exit(7);
		}
		if( !options::have_dbl("cut-infeed") ) {
			cerr << "Error: Board cutting infeed (--cut-infeed) not specified.\n";
			exit(8);
		}

		if( options::dbl("zsafe") <= options::dbl("zcut") ) {
			cerr << "Error: The safety height --zsafe is lower than the cutting "
			     << "height --zcut!\n";
			exit(21);
		}
		if( options::dbl("cut-feed") <= 0 ) {
			cerr << "Error: The cutting feed --cut-feed is <= 0.\n";
			exit(22);
		}
		if( options::dbl("cut-speed") < 0 ) {
			cerr << "Error: The cutting spindle speed --cut-speed is smaller than 0.\n";
			exit(23);
		}
		if( options::dbl("cut-infeed") < 0.001 ) {
			cerr << "Error: Too small cutting infeed --cut-infeed.\n";
			exit(24);
		}
	}
}

void
options::check_parameters()
{
	try {
		check_generic_parameters();
		check_milling_parameters();
		check_cutting_parameters();
		check_drilling_parameters();
	} catch ( std::runtime_error& re ) {
		cerr << "Error: Invalid parameter: " << re.what() << endl;
		exit(100);
	}

	if( !options::have_str("front-output") ) {
		cerr << "Adding front-output.\n";
		instance().sources.begin()->set("front-output", "front.ngc");
	}
}
