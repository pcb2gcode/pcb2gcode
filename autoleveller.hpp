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

// Define this if you want named parameters (only in linuxcnc, easier debug but larger output gcode)
#define AUTOLEVELLER_NAMED_PARAMETERS

//This define controls the number of the bilinear interpolation macro (the "O" code)
#define BILINEAR_INTERPOLATION_MACRO_NUMBER 1000

//This define controls the number of the variable where the interpolation result is saved
#define BILINEAR_INTERPOLATION_RESULT_VAR "100"

#include <string>
using std::string;

#include <fstream>
using std::endl;

#include "coord.hpp"

#include <boost/exception/all.hpp>
class autoleveller_exception: virtual std::exception, virtual boost::exception {
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class autoleveller {
public:
	enum Software { LINUXCNC = 0, MACH3 = 1, MACH4 = 2, TURBOCNC = 3 };

	autoleveller( double xmin, double ymin, double xmax, double ymax, double XProbeDist, double YProbeDist, double zwork, Software software );
	void probeHeader( std::ofstream &of, double zprobe, double zsafe, double zfail, int feedrate, std::string probeOn = "", std::string probeOff = "" );
	void setMillingParameters ( double zwork, double zsafe, int feedrate );
	string addChainPoint ( icoordpair point );
	string g01Corrected ( icoordpair point );
	inline void setLastChainPoint ( icoordpair lastPoint ) {
		this->lastPoint = lastPoint;
	}
	
	const double boardLenX;
	const double boardLenY;
	const double startPointX;
	const double startPointY;
	const unsigned int numXPoints;
	const unsigned int numYPoints;
	const double XProbeDist;
	const double YProbeDist;
	const double averageProbeDist;
	const string zwork;		//Since zwork is only substituted where is necessary, saving it as a string saves lots of double->string conversions
	const Software software;

protected:
	static const char *callSub[];
	static const char *correctedPoint;
	
	icoordpair lastPoint;

	string getVarName( int i, int j );
	static double pointDistance ( icoordpair p0, icoordpair p1 );
	string interpolatePoint ( icoordpair point );
	unsigned int numOfSubsegments ( icoordpair point );
	icoords splitSegment ( const icoordpair point, const unsigned int n );
};

#endif // AUTOLEVELLER_H
