#include "ngc_exporter.hpp"

#include <boost/foreach.hpp>

#include <iostream>
#include <iomanip>
using namespace std;

NGC_Exporter::NGC_Exporter( shared_ptr<Board> board ) : Exporter(board)
{
	this->board = board;
}

void
NGC_Exporter::add_header( string header )
{
        this->header.push_back(header);
}

void
NGC_Exporter::export_all()
{
	BOOST_FOREACH( string layername, board->list_layers() ) {
		cerr << "Current Layer: " << layername << endl;
		export_layer( board->get_layer(layername) );
	}
}

void
NGC_Exporter::export_layer( shared_ptr<Layer> layer )
{
	string layername = layer->get_name();
	shared_ptr<RoutingMill> mill = layer->get_manufacturer();

	// open output file
	std::stringstream of_name; of_name << layername << ".ngc";
	std::ofstream of; of.open( of_name.str().c_str() );

	// write header to .ngc file
        BOOST_FOREACH( string s, header )
        {
                of << "( " << s << " )" << endl;
        }
        of << endl;

        of.setf( ios_base::fixed );
        of.precision(5);
	of << setw(7);

	// preamble
	of << "G94     ( Inches per minute feed rate. )\n"
	   << "G20     ( Units == INCHES.             )\n"
	   << "G90     ( Absolute coordinates.        )\n"
	   << "S" << left << mill->speed << "  ( RPM spindle speed.           )\n"
	   << "M3      ( Spindle on clockwise.        )\n"
	   << endl;

	of << "G64 P0.002 ( set maximum deviation from commanded toolpath )\n"
	   << endl;

	// contours
 	BOOST_FOREACH( shared_ptr<icoords> path, layer->get_toolpaths() )
        {
		// retract, move to the starting point of the next contour
		of << "G00 Z" << mill->zsafe << " ( retract )\n" << endl;
                of << "G00 X" << path->begin()->first << " Y" << path->begin()->second << " ( rapid move to begin. )\n";
		
		/** if we're cutting, perhaps do it in multiple steps, but do isolations just once.
		 *  i know this is partially repetitive, but this way it's easier to read
		 */
		shared_ptr<Cutter> cutter = boost::dynamic_pointer_cast<Cutter>( mill );
		if( cutter && cutter->do_steps ) {
			// cutting
			double z_step = cutter->stepsize;
			double z = mill->zwork + z_step * abs( int( mill->zwork / z_step ) );

			while( z >= mill->zwork ) {
				of << "G01 Z" << z << " F" << mill->feed << " ( plunge. )\n";

				icoords::iterator iter = path->begin();
				while( iter != path->end() ) {
					of << "X" << iter->first << " Y" << iter->second << endl;
					++iter;
				}
			
				z -= z_step;
			}
		} else {
			// isolating
			of << "G01 Z" << mill->zwork << " F" << mill->feed << " ( plunge. )\n";

			icoords::iterator iter = path->begin();
			while( iter != path->end() ) {
				of << "X" << iter->first << " Y" << iter->second << endl;
				++iter;
			}		

		}


		
        }

        of << endl;

	// retract, end
	of << "G00 Z" << mill->zchange << " ( retract )\n" << endl;

	of << "M9 ( Coolant off. )\n";
	of << "M2 ( Program end. )\n\n";

	of.close();
}
