/*!\defgroup AUTOLEVELLER*/
/******************************************************************************/
/*!
 \file       autoleveller.hpp
 \brief
 Autoleveller functions. Idea and hints taken from the output gcode of
 http://www.autoleveller.co.uk/ by James Hawthorne PhD

 \version
 18.12.2014 - Nicola Corna - nicola@corna.info\n
 
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

 \ingroup    AUTOLEVELLER
 */
/******************************************************************************/

#ifndef AUTOLEVELLER_H
#define AUTOLEVELLER_H

// Define this if you want named parameters (only in linuxcnc, easier debug)
#define AUTOLEVELLER_NAMED_PARAMETERS

//This define controls the number of the bilinear interpolation macro (the "O" code)
#define BILINEAR_INTERPOLATION_MACRO_NUMBER 1000

#include <string>
using std::string;

#include <fstream>
using std::endl;

#include <vector>
using std::vector;
using std::pair;

#include <boost/exception/all.hpp>
class autoleveller_exception: virtual std::exception, virtual boost::exception {
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class autoleveller {
public:
	enum Software { LINUXCNC = 0, MACH3 = 1, TURBOCNC = 2 };

	autoleveller( double xmin, double ymin, double xmax, double ymax, double XProbeDist, double YProbeDist, Software software );
	void probeHeader( std::ofstream &of, double zprobe, double zsafe, double zfail, int feedrate, std::string probeOn = "", std::string probeOff = "" );
	void setMillingParameters ( double zdepth, double zsafe, int feedrate );
	string interpolatePoint ( double x, double y );
	
	const double boardLenX;
	const double boardLenY;
	const double startPointX;
	const double startPointY;
	const unsigned int numXPoints;
	const unsigned int numYPoints;
	const double XProbeDist;
	const double YProbeDist;
	const Software software;

protected:
	static const char *callSub[];
	
	double zdepth;
	double zsafe;
	int feedrate;
	double cfactor;

	string getVarName( unsigned int i, unsigned int j );
};

#endif // NGCEXPORTER_H
