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

//Number of the 1st repeat O-code (only for LinuxCNC)
#define REPEAT_CODE_1 10

//Number of the 2nd repeat O-code (only for LinuxCNC)
#define REPEAT_CODE_2 11

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

//Fixed probe fail depth (in inches, string)
#define FIXED_FAIL_DEPTH_IN "-0.2"

//Fixed probe fail depth (in mm, string)
#define FIXED_FAIL_DEPTH_MM "-5"

#include <string>
using std::string;

#include <fstream>
using std::endl;

#include <boost/program_options.hpp>

#include "coord.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class autoleveller {
public:
	enum Software { LINUXCNC = 0, MACH4 = 1, MACH3 = 2, CUSTOM = 3 };

	autoleveller( const boost::program_options::variables_map &options, double quantization_error );
	bool setConfig( std::ofstream &of, std::pair<icoordpair, icoordpair> workarea );
	void header( std::ofstream &of );
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
	
	inline void footer( std::ofstream &of ) {
	    if( software != LINUXCNC )
	        footerNoIf( of );
	}

	//Since some parameters are just placed as string in the output, saving them as
	//strings saves a lot of string to double conversion
	const double XProbeDistRequired;
	const double YProbeDistRequired;
	const string zwork;
	const string zprobe;
    const string zsafe;
	const string zfail;
	const string feedrate;
	const string probeOn;
	const string probeOff;
	const Software software;
    const double quantization_error;

protected:
	double startPointX;
	double startPointY;
	unsigned int numXPoints;
	unsigned int numYPoints;
	double XProbeDist;
	double YProbeDist;
    double averageProbeDist;

	static const char *callSub2[];
    static const char *callInterpolationMacro[];
	static const char *callSubRepeat[];
	string probeCode[4];
	string zProbeResultVar[4];
	string setZZero[4];
	
	icoordpair lastPoint;

	void footerNoIf( std::ofstream &of );
	string getVarName( int i, int j );
	static double pointDistance ( icoordpair p0, icoordpair p1 );
	string interpolatePoint ( icoordpair point );
	unsigned int numOfSubsegments ( icoordpair point );
	icoords splitSegment ( const icoordpair point, const unsigned int n );
};

#endif // AUTOLEVELLER_H
