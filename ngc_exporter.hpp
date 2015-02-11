/*!\defgroup NGC_EXPORTER*/
/******************************************************************************/
/*!
 \file       ngc_exporter.hpp
 \brief

 \version
 09.01.2015 - Nicola Corna - nicola@corna.info\n
 - Added zero-start option
 
 20.11.2014 - Nicola Corna - nicola@corna.info\n
 - added bridge height option
 
  \version
 19.12.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - added option for optimised g-code output (reduces file size up to 40%).
 - added option to add four bridges to the outline cut (clumsy code, but it works)
 - added methods optimise_Path,add_Bridge,get_SlopeOfLine,get_D_PointToLine,get_D_PointToPoint,get_Y_onLine,get_X_onLine

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
#include "autoleveller.hpp"
#include "outline_bridges.hpp"

#define AUTOLEVELLER_FIXED_FAIL_DEPTH ( -0.03937 )

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
	void set_svg_exporter(shared_ptr<SVG_Exporter> svgexpo);
	void set_preamble(string);
	void set_postamble(string);

protected:
	void export_layer(shared_ptr<Layer> layer, string of_name);

	bool bDoSVG;            //!< if true, export svg
	shared_ptr<SVG_Exporter> svgexpo;
	shared_ptr<Board> board;
	vector<string> header;
    string preamble;        //!< Preamble from command line (user file)
    string postamble;       //!< Postamble from command line (user file)

	double g64;             //!< maximum deviation from commanded toolpath
	double cfactor;         //!< imperial/metric conversion factor for output file
	bool bMetricinput;      //!< if true, input parameters are in metric units
	bool bMetricoutput;     //!< if true, metric g-code output
	bool bMirrored;         //!< if true, mirrored along y axis
	bool bCutfront;         //!< if true, the outline will be cut from front

	outline_bridges *bridges;
	
	autoleveller *leveller;
	double autolevellerFeed;
	double autolevellerFailDepth;
	bool bFrontAutoleveller;
	bool bBackAutoleveller;
	string probeOnCommands;
	string probeOffCommands;

	double xoffset;
	double yoffset;
};

#endif // NGCEXPORTER_H
