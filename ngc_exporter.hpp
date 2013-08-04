/*!\defgroup NGC_EXPORTER*/
/******************************************************************************/
/*!
 \file       ngc_exporter.hpp
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

#ifndef NGCEXPORTER_H
#define NGCEXPORTER_H

#include <vector>
using std::vector;

#include <string>
using std::string;
using std::pair;

#include <fstream>
using std::ofstream;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <boost/program_options.hpp>

#include "coord.hpp"
#include "mill.hpp"
#include "exporter.hpp"
#include "svg_exporter.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class NGC_Exporter: public Exporter {
public:
	NGC_Exporter(shared_ptr<Board> board);

	/* virtual void add_path( shared_ptr<icoords> ); */
	/* virtual void add_path( vector< shared_ptr<icoords> > ); */

	void add_header(string);
	void export_all(boost::program_options::variables_map&);

	//SVG EXPORTER
	void set_svg_exporter(shared_ptr<SVG_Exporter> svgexpo);

	void set_preamble(string);
	void set_postamble(string);

protected:
	double get_tolerance(void);
	void export_layer(shared_ptr<Layer> layer, string of_name);

	//SVG EXPORTER
	bool bDoSVG;
	shared_ptr<SVG_Exporter> svgexpo;

	shared_ptr<Board> board;
	vector<string> header;
	string preamble, postamble;

	double g64; //!< maximum deviation from commanded toolpath [inch]
	bool metricinput; //!< if true, input parameters are in metric units
	bool metricoutput; //!< if true, metric g-code output
};

#endif // NGCEXPORTER_H
