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

//Number of the bilinear interpolation macro
#define BILINEAR_INTERPOLATION_MACRO_NUMBER 5

//Number of the correction factor subroutine
#define CORRECTION_FACTOR_SUB_NUMBER 6

//Number of the g01 interpolated macro (just a shortcut for CORRECTION_FACTOR_SUB_NUMBER + G01 X Y)
#define G01_INTERPOLATED_MACRO_NUMBER 7

//Number of the Y probe subroutine
#define YPROBE_SUB_NUMBER 8

//Number of the X probe subroutine
#define XPROBE_SUB_NUMBER 9

//Number of the "repeat" N-code (turboCNC) or O-code (LinuxCNC) - also
//REPEAT_CODE + 10, REPEAT_CODE + 200 and REPEAT_CODE + 210 will be used
#define REPEAT_CODE 555

//Name of the variable where the interpolation result is saved (string)
#define RESULT_VAR "100"

//Name of the 1st usable global variable (string)
#define GLOB_VAR_0 "110"

//Name of the 2nd usable global variable (string)
#define GLOB_VAR_1 "111"

//Name of the 3rd usable global variable (string)
#define GLOB_VAR_2 "112"

//Name of the 4th usable global variable (string)
#define GLOB_VAR_3 "113"

//Name of the 5th usable global variable (string)
#define GLOB_VAR_4 "114"

//Name of the 6th usable global variable (string)
#define GLOB_VAR_5 "115"

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
	enum Software { LINUXCNC = 0, MACH4 = 1, MACH3 = 2, TURBOCNC = 3 };

	autoleveller( double XProbeDist, double YProbeDist, double zwork, Software software );
	void probeHeader( std::ofstream &of, std::pair<icoordpair, icoordpair> workarea, double zprobe, double zsafe, double zfail, int feedrate, std::string probeOn = "", std::string probeOff = "" );
	void setMillingParameters ( double zwork, double zsafe, int feedrate );
	string addChainPoint ( icoordpair point );
	string g01Corrected ( icoordpair point );
	string getSoftware();
	
	inline void setLastChainPoint ( icoordpair lastPoint ) {
		this->lastPoint = lastPoint;
	}
	inline unsigned int maxProbePoints() {
	    return software == LINUXCNC ? 4501 : 500;
	}
	inline unsigned int requiredProbePoints() {
	    return numXPoints * numYPoints;
	}
	
	const double XProbeDistRequired;
	const double YProbeDistRequired;	
	const string zwork;		//Since zwork is only substituted where is necessary, saving it as a string saves lots of double->string conversions
	const Software software;

protected:
    double workareaLenX;
	double workareaLenY;
	double startPointX;
	double startPointY;
	unsigned int numXPoints;
	unsigned int numYPoints;
	double XProbeDist;
	double YProbeDist;
    double averageProbeDist;

	static const char *callSub2[];
    static const char *callInterpolationMacro[];
	
	icoordpair lastPoint;

	string getVarName( int i, int j );
	static double pointDistance ( icoordpair p0, icoordpair p1 );
	string interpolatePoint ( icoordpair point );
	unsigned int numOfSubsegments ( icoordpair point );
	icoords splitSegment ( const icoordpair point, const unsigned int n );
};

#endif // AUTOLEVELLER_H
