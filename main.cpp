
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <gtkmm/main.h>

#include <glibmm/ustring.h>
using Glib::ustring;

#include "gerberimporter.hpp"
#include "surface.hpp"
#include "ngc_exporter.hpp"
#include "board.hpp"
#include "drill.hpp"
#include "options.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include <fstream>
#include <sstream>

void check_milling_parameters( po::variables_map& vm );
void check_drilling_parameters( po::variables_map& vm );
void check_cutting_parameters( po::variables_map& vm );

void check_parameters( po::variables_map& vm )
{
	int dpi = vm["dpi"].as<int>();
	if( dpi < 100 ) cerr << "Warning: very low DPI value." << endl;
	if( dpi > 10000 ) cerr << "Warning: very high DPI value, processing may take extremely long" << endl;

	if( !vm.count("zsafe") ) {
		cerr << "Error: Safety height not specified.\n";
		exit(5);
	}

	if( !vm.count("front") && !vm.count("back") ) {
		if( vm.count("outline") )
			cerr << "Warning: Neither --front nor --back specified.\n";
		else {
			cerr << "Error: No input files specified.\n";
			exit(2);
		}
	} else {
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


int main( int argc, char* argv[] )
{
	// parse and check command line options
	Gtk::Main kit(argc, argv);
	
	options::parse_cl( argc, argv );
	po::variables_map& vm = options::get_vm();

	if( vm.count("version") ) {
		cout << PACKAGE_STRING << endl;
		exit(0);
	}

	if( vm.count("help") ) {
		cout << options::help();
		exit(0);
		
		// cout << endl << "If you're new to pcb2gcode and CNC milling, please don't forget to read the attached documentation! "
		//      << "It contains lots of valuable hints on both using this program and milling circuit boards." << endl;
	}

	check_parameters( vm );

	
	// prepare environment
	shared_ptr<Isolator> isolator( new Isolator() );
	isolator->tool_diameter = vm["offset"].as<double>() * 2;
	isolator->zwork = vm["zwork"].as<double>();
	isolator->zsafe = vm["zsafe"].as<double>();
	isolator->feed = vm["mill-feed"].as<double>();
	isolator->speed = vm["mill-speed"].as<int>();
	isolator->zchange = vm["zchange"].as<double>();

	shared_ptr<Cutter> cutter( new Cutter() );
	cutter->tool_diameter = vm["cutter-diameter"].as<double>() - 2 * 0.005; // 2*0.005 compensates for the 10 mil outline, read doc/User_Manual.pdf
	cutter->zwork = vm["zcut"].as<double>();
	cutter->zsafe = vm["zsafe"].as<double>();
	cutter->feed = vm["cut-feed"].as<double>();
	cutter->speed = vm["cut-speed"].as<int>();
	cutter->zchange = vm["zchange"].as<double>();
	cutter->do_steps = true;
	cutter->stepsize = vm["cut-infeed"].as<double>();

	shared_ptr<Driller> driller( new Driller() );
	if( vm.count("drill") ) {
		driller->zwork = vm["zdrill"].as<double>();
		driller->zsafe = vm["zsafe"].as<double>();
		driller->feed = vm["drill-feed"].as<double>();
		driller->speed = vm["drill-speed"].as<int>();
		driller->zchange = vm["zchange"].as<double>();
	}


	shared_ptr<Board> board( new Board() );

	if( vm.count("margins") )
		board->set_margins( vm["margins"].as<double>() );

	// load files
	try
	{
		// import layer files, create surface
		cout << "Importing front side... ";
		try {
			string frontfile = vm["front"].as<string>();
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(frontfile) );
			board->prepareLayer( "front", importer, isolator, false );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( boost::exception& e ) {
			cout << "not specified\n";
		}

		cout << "Importing back side... ";
		try {
			string backfile = vm["back"].as<string>();
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(backfile) );
			board->prepareLayer( "back", importer, isolator, true );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( boost::exception& e ) {
			cout << "not specified\n";
		}

		cout << "Importing outline... ";
		try {
			string outline = vm["outline"].as<string>();
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(outline) );
			board->prepareLayer( "outline", importer, cutter, !vm.count("front") );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( boost::exception& e ) {
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

	board->createLayers();

	cout << "Calculated board dimensions: " << board->get_width() << "in x " << board->get_height() << "in" << endl;

	try {
		shared_ptr<NGC_Exporter> exporter( new NGC_Exporter( board ) );
		exporter->add_header( PACKAGE_STRING );
		exporter->export_all();
	} catch( std::logic_error& le ) {
		cout << "Internal Error: " << le.what() << endl;
	}

	if( vm.count("drill") ) {
		cout << "Converting " << vm["drill"].as<string>() << "... ";

		ExcellonProcessor ep( vm["drill"].as<string>(), board->get_min_x() + board->get_max_x() );
		ep.add_header( PACKAGE_STRING );
		ep.export_ngc( vm["drill"].as<string>(), driller );

		cout << "done.\n";
	}


}
