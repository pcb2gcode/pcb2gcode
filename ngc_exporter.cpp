
/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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
NGC_Exporter::export_all(boost::program_options::variables_map& options)
{
	BOOST_FOREACH( string layername, board->list_layers() ) {
		std::stringstream option_name;
		option_name << layername << "-output";
		string of_name = options[option_name.str()].as<string>();
		cerr << "Current Layer: " << layername << ", exporting to " << of_name << "." << endl;
		export_layer( board->get_layer(layername), of_name);
	}
}


void
NGC_Exporter::export_layer( shared_ptr<Layer> layer, string of_name )
{
	string layername = layer->get_name();
	shared_ptr<RoutingMill> mill = layer->get_manufacturer();
    double last_x = -99.99;
    double last_y = -99.99;
    bool skipped = FALSE;
    bool skip_x = FALSE;
    bool skip_y = FALSE;
    bool lastskip_x = FALSE;
    bool lastskip_y = FALSE;

	// open output file
	std::ofstream of; of.open( of_name.c_str() );

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
        last_x = -99.99;
        last_y = -99.99;
        skipped = FALSE;
        skip_x = FALSE;
        lastskip_x = FALSE;
        skip_y = FALSE;
        lastskip_y = FALSE;
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
				    lastskip_x = skip_x;
				    lastskip_y = skip_y;
				    skip_x = ( iter->first == last_x ) ? TRUE : FALSE;
				    skip_y = ( iter->second == last_y ) ? TRUE : FALSE;
                    if ( ( !skip_x && skip_x == lastskip_x ) ||
                         ( !skip_y && skip_y == lastskip_y ) ) {
                        skipped = TRUE;
                    }
                    else {
					    of << "X" << iter->first << " Y" << iter->second << endl;
                        skipped = FALSE;
                    }
                    last_x = iter->first;
                    last_y = iter->second;
					++iter;
				    if (iter == path->end() && skipped ) {
				        of << "X" << last_x << " Y" << last_y << endl;
				    }
				}
			
				z -= z_step;
			}
		} else {
			// isolating
			of << "G01 Z" << mill->zwork << " F" << mill->feed << " ( plunge. )\n";

			icoords::iterator iter = path->begin();
			while( iter != path->end() ) {
				lastskip_x = skip_x;
				lastskip_y = skip_y;
				skip_x = ( iter->first == last_x ) ? TRUE : FALSE;
				skip_y = ( iter->second == last_y ) ? TRUE : FALSE;
                if ( ( !skip_x && skip_x == lastskip_x ) ||
                     ( !skip_y && skip_y == lastskip_y ) ) {
                    skipped = TRUE;
                }
                else {
					of << "X" << iter->first << " Y" << iter->second << endl;
                    skipped = FALSE;
                }
                last_x = iter->first;
                last_y = iter->second;
				++iter;
				if (iter == path->end() && skipped ) {
				    of << "X" << last_x << " Y" << last_y << endl;
				}
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

void NGC_Exporter::set_preamble(string _preamble)
{
	preamble=_preamble;
}

void NGC_Exporter::set_postamble(string _postamble)
{
	postamble=_postamble;
}
