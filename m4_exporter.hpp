/*!\defgroup M4_EXPORTER*/
/******************************************************************************/
/*!
 \file       m4_exporter.hpp
 \brief

 \version
 based on latest 1.1.4 - 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> 

 \ingroup    M4_EXPORTER
 */
/******************************************************************************/

#ifndef M4EXPORTER_H
#define M4EXPORTER_H

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

/******************************************************************************/
/*
 */
/******************************************************************************/
class M4_Exporter: public Exporter {
public:
	M4_Exporter(shared_ptr<Board> board);
	/* virtual void add_path( shared_ptr<icoords> ); */
	/* virtual void add_path( vector< shared_ptr<icoords> > ); */
	void add_header(string);
	void export_all(boost::program_options::variables_map&);
	void set_svg_exporter(shared_ptr<SVG_Exporter> svgexpo);
	void set_preamble(string);
	void set_postamble(string);

protected:
	void export_layer(shared_ptr<Layer> layer, string of_name);
	inline bool aligned(icoords::const_iterator p0, icoords::const_iterator p1, icoords::const_iterator p2) {
	    return ( (p0->first == p1->first) && (p1->first == p2->first) ) ||      //x-aligned
	           ( (p0->second == p1->second) && (p1->second == p2->second) );    //y-aligned
	}

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
    bool bBridges;
    const unsigned int dpi;
    const double quantization_error;

	
	autoleveller *leveller;
	bool bFrontAutoleveller;
	bool bBackAutoleveller;

	double xoffset;
	double yoffset;
};

#endif // M4EXPORTER_H
