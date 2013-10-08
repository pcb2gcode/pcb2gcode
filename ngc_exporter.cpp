/*!\defgroup NGC_EXPORTER*/
/******************************************************************************/
/*!
 \file       ngc_exporter.cpp
 \brief

 \version
 04.08.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Added metricoutput option.
 - Added g64 option.
 - Added ondrill option.
 - Started documenting the code for doxygen processing.
 - Formatted the code with the Eclipse code styler (Style: K&R).

 \version
 1.1.4 - 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others

 \copyright  pcb2gcode is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 pcb2gcode is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.

 \ingroup    NGC_EXPORTER
 */
/******************************************************************************/

#include "ngc_exporter.hpp"
#include <boost/foreach.hpp>
#include <iostream>
#include <iomanip>
using namespace std;

/******************************************************************************/
/*
 */
/******************************************************************************/
NGC_Exporter::NGC_Exporter(shared_ptr<Board> board) : Exporter(board) {
	this->board = board;
	bDoSVG = false;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_svg_exporter(shared_ptr<SVG_Exporter> svgexpo) {
	this->svgexpo = svgexpo;
	bDoSVG = true;
}
/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::add_header(string header) {
	this->header.push_back(header);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::export_all(boost::program_options::variables_map& options) {
	metricinput = options["metric"].as<bool>(); //set flag for metric input
	metricoutput = options["metricoutput"].as<bool>(); //set flag for metric output
	g64 = options.count("g64") ? options["g64"].as<double>() : 1000; //set g64 value

	if (metricinput)
		g64 /= 25.4; //convert to inches if needed 

	BOOST_FOREACH( string layername, board->list_layers() ) {
		std::stringstream option_name;
		option_name << layername << "-output";
		string of_name = options[option_name.str()].as<string>();
		cerr << "Exporting " << layername << "... ";
		export_layer(board->get_layer(layername), of_name);
		cerr << "DONE." << endl;
	}
}

/******************************************************************************/
/*
 \brief  Returns the maximum allowed tolerance for the commanded toolpath
 \retval Maximum allowed tolerance in Inches.
 */
/******************************************************************************/
double NGC_Exporter::get_tolerance(void) {
	if (g64 < (1 / 25.4)) {
		return g64; // return g64 value if plausible value (more or less) is given.
	} else {
		return 5.0 / this->board->get_dpi(); // set maximum deviation to 5 pixels to ensure smooth movement
	}
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::export_layer(shared_ptr<Layer> layer, string of_name) {
	string layername = layer->get_name();
	shared_ptr<RoutingMill> mill = layer->get_manufacturer();

	bool bSvgOnce = TRUE;

	double factor; //!< the output coordinates are multiplied with this factor

	//set multiplication factor for output coordinates depending on metric flag
	if (metricoutput) {
		factor = 25.4;
	} else {
		factor = 1;
	}

	// open output file
	std::ofstream of;
	of.open(of_name.c_str());

	// write header to .ngc file
	BOOST_FOREACH( string s, header ) {
		of << "( " << s << " )" << endl;
	}
	of << endl;

	of.setf(ios_base::fixed);
	of.precision(5);
	of << setw(7);

	// preamble
	of << preamble;

	if (metricoutput) {
		of << "G94     ( Millimeters per minute feed rate. )\n"
			<< "G21     ( Units == Millimeters. )\n" << endl;
	} else {
		of << "G94     ( Inches per minute feed rate. )\n"
			<< "G20     ( Units == INCHES. )\n" << endl;
	}

	of << "G90     ( Absolute coordinates.        )\n" << "S" << left
		<< mill->speed << "  ( RPM spindle speed.           )\n"
		<< "M3      ( Spindle on clockwise.        )\n" << endl;

	of << "G64 P" << get_tolerance() * factor
		<< " ( set maximum deviation from commanded toolpath )\n" << endl;

	//SVG EXPORTER
	if (bDoSVG) {
		//choose a color
		svgexpo->set_rand_color();
	}

	// contours
	BOOST_FOREACH( shared_ptr<icoords> path, layer->get_toolpaths() ) {
		// retract, move to the starting point of the next contour
		of
			<< "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
		of << "G00 Z" << mill->zsafe * factor << " ( retract )\n" << endl;
		of << "G00 X" << path->begin()->first * factor << " Y" << path->begin()->second * factor << " ( rapid move to begin. )\n";

		//SVG EXPORTER
		if (bDoSVG) {
			svgexpo->move_to(path->begin()->first, path->begin()->second);
			bSvgOnce = TRUE;
		}

		/** if we're cutting, perhaps do it in multiple steps, but do isolations just once.
		 *  i know this is partially repetitive, but this way it's easier to read
		 */
		shared_ptr<Cutter> cutter = boost::dynamic_pointer_cast<Cutter>(mill);

		if (cutter && cutter->do_steps) {
			// cutting
			double z_step = cutter->stepsize;
			double z = mill->zwork + z_step * abs(int(mill->zwork / z_step));

			while (z >= mill->zwork) {
				of << "G01 Z" << z * factor << " F" << mill->feed * factor
					<< " ( plunge. )\n";
				of
					<< "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";

				icoords::iterator iter = path->begin();
				icoords::iterator last = path->end(); // initializing to quick & dirty sentinel value
				icoords::iterator peek;

				while (iter != path->end()) {
					peek = iter + 1;

					if ( /* it's necessary to write the coordinates if... */
					last == path->end() || /* it's the beginning */
					peek == path->end()	|| /* it's the end */
					!( /* or if neither of the axis align */
							(last->first == iter->first	&& iter->first == peek->first) || /* x axis aligns */
							(last->second == iter->second && iter->second == peek->second) /* y axis aligns */	)
						/* no need to check for "they are on one axis but iter is outside of last and peek" because that's impossible from how they are generated */
						) {
						of << "X" << iter->first * factor << " Y" << iter->second * factor << endl;

						//SVG EXPORTER
						if (bDoSVG) {
							if (bSvgOnce)
								svgexpo->line_to(iter->first, iter->second);
						}
					}

					last = iter;
					++iter;
				}

				//SVG EXPORTER
				if (bDoSVG) {
					svgexpo->close_path();
					bSvgOnce = FALSE;
				}

				z -= z_step;
			}
		} else {
			// isolating
			of << "G01 Z" << mill->zwork * factor << " F" << mill->feed * factor
				<< " ( plunge. )\n";
			of
				<< "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";

			icoords::iterator iter = path->begin();
			icoords::iterator last = path->end(); // initializing to quick & dirty sentinel value
			icoords::iterator peek;

			while (iter != path->end()) {
				peek = iter + 1;

				if ( /* it's necessary to write the coordinates if... */
				last == path->end() || /* it's the beginning */
				peek == path->end() || /* it's the end */
				!( /* or if neither of the axis align */
				(last->first == iter->first && iter->first == peek->first) || /* x axis aligns */
				(last->second == iter->second && iter->second == peek->second) /* y axis aligns */
				)
				/* no need to check for "they are on one axis but iter is outside of last and peek" becaus that's impossible from how they are generated */
				) {
					of << "X" << iter->first * factor << " Y"
						<< iter->second * factor << endl;

					//SVG EXPORTER
					if (bDoSVG)
						if (bSvgOnce)
							svgexpo->line_to(iter->first, iter->second);

				}

				last = iter;
				++iter;
			}

			//SVG EXPORTER
			if (bDoSVG) {
				svgexpo->close_path();
				bSvgOnce = FALSE;
			}

		}

	}

	of << endl;
	// retract
	of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )" << endl;
	of << "G00 Z" << mill->zchange * factor << " ( retract )\n" << endl;
	of << postamble;
	of << "M9 ( Coolant off. )" << endl;
	of << "M2 ( Program end. )" << endl;
	of.close();

	//SVG EXPORTER
	if (bDoSVG) {
		svgexpo->stroke();
	}
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_preamble(string _preamble) {
	preamble = _preamble;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_postamble(string _postamble) {
	postamble = _postamble;
}

