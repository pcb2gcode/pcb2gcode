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


#include <iostream>
#include <fstream>

using std::cout;
using std::cerr;
using std::endl;
using std::fstream;

#include <glibmm/ustring.h>
using Glib::ustring;

#include <glibmm/init.h>
#include <gdkmm/wrap_init.h>

#include "gerberimporter.hpp"
#include "surface.hpp"
#include "ngc_exporter.hpp"
#include "board.hpp"
#include "drill.hpp"
#include "options.hpp"
#include "svg_exporter.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include <fstream>
#include <sstream>


int main( int argc, char* argv[] )
{
	Glib::init();
	Gdk::wrap_init();
	
	options::parse( argc, argv );
	options::parse( "millproject" );
	
	if( options::cl_flag("version") ) {
		cout << PACKAGE_STRING << endl;
		exit(0);
	}

	if( options::cl_flag("help") ) {
		cout << options::help();
		exit(0);

		// cout << endl << "If you're new to pcb2gcode and CNC milling, please don't forget to read the attached documentation! "
		//      << "It contains lots of valuable hints on both using this program and milling circuit boards." << endl;
	}

	options::check_parameters();

	double unit=1;
	if( options::flag("metric") ) {
		unit=1./25.4;
	}


	// prepare environment
	shared_ptr<Isolator> isolator;
	if( options::have_str("front") || options::have_str("back") ) {
		isolator = shared_ptr<Isolator>( new Isolator() );
		isolator->tool_diameter = options::dbl("offset") * 2*unit;
		isolator->zwork = options::dbl("zwork")*unit;
		isolator->zsafe = options::dbl("zsafe")*unit;
		isolator->feed = options::dbl("mill-feed")*unit;
		isolator->speed = options::dbl("mill-speed");
		isolator->zchange = options::dbl("zchange")*unit;
		isolator->extra_passes = options::have_dbl("extra-passes") ? options::dbl("extra-passes") : 0;
	}

	shared_ptr<Cutter> cutter;
	if( options::have_str("outline") || (options::have_str("drill") && options::flag("milldrill")) ) {
		cutter = shared_ptr<Cutter>( new Cutter() );
		cutter->tool_diameter = options::dbl("cutter-diameter") * unit;
		cutter->zwork = options::dbl("zcut") * unit;
		cutter->zsafe = options::dbl("zsafe")*unit;
		cutter->feed = options::dbl("cut-feed")*unit;
		cutter->speed = options::dbl("cut-speed");
		cutter->zchange = options::dbl("zchange")*unit;
		cutter->do_steps = true;
		cutter->stepsize = options::dbl("cut-infeed")*unit;
	}

	shared_ptr<Driller> driller;
	if( options::have_str("drill") ) {
		driller = shared_ptr<Driller>( new Driller() );
		driller->zwork = options::dbl("zdrill")*unit;
		driller->zsafe = options::dbl("zsafe")*unit;
		driller->feed = options::dbl("drill-feed")*unit;
		driller->speed = options::dbl("drill-speed");
		driller->zchange = options::dbl("zchange")*unit;
	}

	// prepare custom preamble
	string preamble, postamble;
/*	if( options::have_str("preamble"))
	{
		string name = vm["preamble"].as<string>();
		fstream in(name.c_str(),fstream::in);
		if(!in.good())
		{
			cerr << "Cannot read preamble file \"" << name << "\"" << endl;
			exit(1);
		}
		string tmp((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		preamble = tmp + "\n\n";
	}

	if( vm.count("postamble"))
	{
		string name = vm["postamble"].as<string>();
		fstream in(name.c_str(),fstream::in);
		if(!in.good())
		{
			cerr << "Cannot read preamble file \"" << name << "\"" << endl;
			exit(1);
		}
		string tmp((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		postamble = tmp + "\n\n";
		} */

	shared_ptr<Board> board(new Board( options::dbl("dpi"), options::flag("fill-outline"),
					   options::flag("fill-outline") ? options::dbl("outline-width") : INFINITY ));

	// this is currently disabled, use --outline instead
/*	if( vm.count("margins") )
		board->set_margins( vm["margins"].as<double>() );
*/

	// load files
	try
	{
		// import layer files, create surface
		cout << "Importing front side... ";
		try {
			string frontfile = options::str("front");
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(frontfile) );
			board->prepareLayer( "front", importer, isolator, false, options::flag("mirror-absolute") );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( std::runtime_error& e ) {
			cout << "not specified\n";
		}

		cout << "Importing back side... ";
		try {
			string backfile = options::str("back");
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(backfile) );
			board->prepareLayer( "back", importer, isolator, true, options::flag("mirror-absolute") );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( std::runtime_error& e ) {
			cout << "not specified\n";
		}

		cout << "Importing outline... ";
		try {
			string outline = options::str("outline");
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(outline) );
			board->prepareLayer( "outline", importer, cutter, !options::have_str("front"), options::flag("mirror-absolute") );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( std::runtime_error& e ) {
			cout << "not specified\n";
		}

	}
	catch(import_exception ie)
	{
		if( ustring const* mes = boost::get_error_info<errorstring>(ie) )
			std::cerr << "Import Error: " << *mes;
		else
			std::cerr << "Import Error: No reason given.";
	}
	
	//SVG EXPORTER
	shared_ptr<SVG_Exporter> svgexpo( new SVG_Exporter( board ) );
	
	try {
		board->createLayers();   // throws std::logic_error
		cout << "Calculated board dimensions: " << board->get_width() << "in x " << board->get_height() << "in" << endl;
		
		//SVG EXPORTER
		if( options::have_str("svg") ) {
			cout << "Create SVG File ... " << options::str("svg") << endl;
			svgexpo->create_svg( options::str("svg") );
		}
		
		shared_ptr<NGC_Exporter> exporter( new NGC_Exporter( board ) );
		exporter->add_header( PACKAGE_STRING );

		if( options::have_str("preamble") ) exporter->set_preamble(preamble);
		if( options::have_str("postamble") ) exporter->set_postamble(postamble);
		
		//SVG EXPORTER
		if( options::have_str("svg") ) exporter->set_svg_exporter( svgexpo );
		
		exporter->export_all();
	} catch( std::logic_error& le ) {
		cout << "Internal Error: " << le.what() << endl;
	} catch( std::runtime_error& re ) {
	}

	if( options::have_str("drill") ) {
		cout << "Converting " << options::str("drill") << "... ";
		try {
			ExcellonProcessor ep( options::str("drill"), board->get_min_x() + board->get_max_x() );
			ep.add_header( PACKAGE_STRING );

			//SVG EXPORTER
			if( options::flag("svg") ) ep.set_svg_exporter( svgexpo );
			
			if( options::flag("milldrill") )
				ep.export_ngc( options::str("drill-output", "drill.ngc"), cutter, !options::flag("drill-front"), options::flag("mirror-absolute") );
			else
				ep.export_ngc( options::str("drill-output", "drill.ngc"), driller, !options::flag("drill-front"), options::flag("mirror-absolute") );

			cout << "done.\n";
		} catch( drill_exception& e ) {
			cout << "ERROR.\n";
		}
	} else {
		cout << "No drill file specified.\n";
	}
}
