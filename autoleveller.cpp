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

#include "autoleveller.hpp"

#include <sstream>
#include <cmath>
#include <boost/algorithm/string/replace.hpp>
#include <boost/format.hpp>

/*	^ y
 *	|
 *	|	A	B
 *	|
 *	|	C	D
 *	|		    x
 *	+----------->
 *
 * This string calls the o-th macro. The order of the argument is:
 * 1 - number of the macro
 * 2 - point A
 * 3 - point B
 * 4 - point C
 * 5 - point D
 * 6 - distance between the y coordinate of the point and the y coordinate of C/D
 * 6 - distance between the x coordinate of the point and the x coordinate of A/C
 */

const char *autoleveller::callSub[] = { "o%1$s call [%2$s] [%3$s] [%4$s] [%5$s] [%6$.5f] [%7$.5f]",
					 	  				"G65 P%1$s A%2$s B%3$s C%4$s I%5$s J%6$.5f K%7$.5f",
						 				"G65 P%1$s A%2$s B%3$s C%4$s I%5$s J%6$.5f K%7$.5f" };
						 				  
/******************************************************************************/
/*	
 *  Constructor, throws an exception if the number of required variables (probe
 *  points) exceeds the maximum number of variables (unlimited for linuxCNC)
 */
/******************************************************************************/
autoleveller::autoleveller(double xmin, double ymin, double xmax, double ymax, double XProbeDist, double YProbeDist, Software software) :
 boardLenX( xmax - xmin ),
 boardLenY( ymax - ymin ),
 startPointX( xmin ),
 startPointY( ymin ),
 numXPoints( round ( boardLenX / XProbeDist ) > 1 ? round ( boardLenX / XProbeDist ) + 1 : 2 ),		//We need at least 2 probe points per axis
 numYPoints( round ( boardLenY / YProbeDist ) > 1 ? round ( boardLenY / YProbeDist ) + 1 : 2 ),
 XProbeDist( boardLenX / ( numXPoints - 1 ) ),
 YProbeDist( boardLenY / ( numYPoints - 1 ) ),
 software( software )
{
	if( numXPoints * numYPoints > 500 )
		throw autoleveller_exception();
}

/******************************************************************************/
/*
 *  Returns the ij-th variable name (based on software)
 */
/******************************************************************************/
string autoleveller::getVarName( unsigned int i, unsigned int j ) {
	std::stringstream ss;

#ifdef AUTOLEVELLER_NAMED_PARAMETERS
	if ( software == LINUXCNC )
		ss << "#<_" << i << '_' << j << '>';	//getVarName(10,8) returns #<_10_8>	
	else
#endif
		ss << '#' << i * numYPoints + j + 500;	//getVarName(10,8) returns (numYPoints=10) #180

	return ss.str();
}

/******************************************************************************/
/*
 *  Generate the gcode probe header
 */
/******************************************************************************/
void autoleveller::probeHeader( std::ofstream &of, double zprobe, double zsafe, double zfail, int feedrate, std::string probeOn, std::string probeOff ) {
	const char *probeCode[] = { "G38.2", "G31", "G31" };
	const char *setZero[] = { "G10 L20 P0 Z0", "G92 Z0", "G92 Z0" };
	const char *zProbeResultVar[] = { "#5063", "#2002", "#2002" };
	const char *logFileOpenAndComment[] =
	 { "(PROBEOPEN RawProbeLog.txt) ( Record all probes in RawProbeLog.txt )",
	   "M40 (Begins a probe log file, when the window appears, enter a name for the log file such as \"RawProbeLog.txt\")",
	   "( No probe log function available in turboCNC )" };
	const char *logFileClose[] = { "(PROBECLOSE)" , "M41", "( No probe log function available in turboCNC )" };
#ifdef AUTOLEVELLER_NAMED_PARAMETERS
	const char *parameterForm[] = { "parameter in the form #<_xprobenumber_yprobenumber>", "numbered parameter", "numbered parameter" };
#else
	const char *parameterForm[] = { "numbered parameter", "numbered parameter", "numbered parameter" };
#endif

	const char *startSub[] = { "o%1$d sub", "O%1$d", "O%1$d" };
	const char *endSub[] = { "o%1$d endsub", "M99 ( end of sub number %1$d )", "M99  ( end of sub number %1$d )" };

	int i;
	int j = 0;
	int incr_decr = 1;

	boost::replace_all(probeOn, "$", "\n");
	boost::replace_all(probeOff, "$", "\n");

	of << boost::format(startSub[software]) % BILINEAR_INTERPOLATION_MACRO_NUMBER << " ( Bilinear interpolation macro )" << endl;
	of << "#7=[#3+[#1-#3]*#5] ( Linear interpolation of the x-min elements )" << endl;
	of << "#8=[#4+[#2-#4]*#5] ( Linear interpolation of the x-max elements )" << endl;
	of << "#100=[#7+[#8-#7]*#6] ( Linear interpolation of previously interpolated points )" << endl;
	of << boost::format(endSub[software]) % BILINEAR_INTERPOLATION_MACRO_NUMBER << endl;
	of << endl;
	of << probeOn << endl;
	of << "G0 Z" << zsafe << " ( Move Z to safe height )"<< endl;
	of << "G0 X" << startPointX << " Y" << startPointY << " ( Move XY to start point )" << endl;
	of << "G0 Z" << zprobe << " ( Move Z to probe height )" << endl;
	of << probeCode[software] << " Z" << zfail << " F" << feedrate << " ( Z-probe )" << endl;
	of << setZero[software] << " ( Set the current Z as zero-value )" << endl;
	of << "G0 Z" << zprobe << " ( Move Z to probe height )" << endl;
	of << probeCode[software] << " Z" << zfail << " F" << feedrate / 2 << " ( Z-probe again, half speed )" << endl;
	of << setZero[software] << " ( Set the current Z as zero-value )" << endl;
	of << logFileOpenAndComment[software] << endl;
	of << endl;
	of << "( We now start the real probing: move the Z axis to the probing height, move to )" << endl;
	of << "( the probing XY position, probe it and save the result, parameter " << zProbeResultVar[software] << ", )" << endl;
	of << "( in a " << parameterForm[software] << " )" << endl;
	of << "( We will make " << numXPoints << " probes on the X-axis and " << numYPoints << " probes on the Y-axis, )" << endl;
	of << "( for a total of " << numXPoints * numYPoints << " probes )" << endl;
	of << endl;

	for( int i = 0; i < numXPoints; i++ ) {
		while( j >= 0 && j < numYPoints ) {
			of << "G0 Z" << zprobe << endl;			//Move Z to probe height
			of << "X" << i * XProbeDist + startPointX << " Y" << j * YProbeDist + startPointY << endl;		//Move to the XY coordinate
			of << probeCode[software] << " Z" << zfail << " F" << feedrate << endl;	//Z-probe
			of << getVarName(i, j) << "=" << zProbeResultVar[software] << endl;	//Save the Z-value

			j += incr_decr ;
		}
		incr_decr *= -1;
		j += incr_decr ;
	}

	of << "G0 Z" << zsafe << " ( Move Z to safe height )"<< endl;
	of << logFileClose[software] << " ( Close the probe log file )" << endl;
	of << "( Probing has ended, each Z-coordinate will be corrected with a bilinear interpolation )" << endl;
	of << probeOff << endl;
	of << endl;
}

string autoleveller::interpolatePoint ( double x, double y ) {
	int xminindex;
	int yminindex;
	double x_minus_x0;
	double y_minus_y0;
	
	xminindex = floor ( ( x - startPointX ) / XProbeDist );
	yminindex = floor ( ( y - startPointY ) / YProbeDist );
	x_minus_x0 = x - startPointX - xminindex * XProbeDist;
	y_minus_y0 = y - startPointY - yminindex * YProbeDist;
	
	return str ( boost::format( callSub[software] ) % BILINEAR_INTERPOLATION_MACRO_NUMBER %
													  getVarName( xminindex, yminindex + 1 ) %
												      getVarName( xminindex + 1, yminindex + 1 ) %
												      getVarName( xminindex, yminindex ) %
												      getVarName( xminindex + 1, yminindex ) %
												      y_minus_y0 % x_minus_x0 );
}

void autoleveller::setMillingParameters ( double zdepth, double zsafe, int feedrate ) {
	this->zdepth = zdepth;
	this->zsafe = zsafe;
	this->feedrate = feedrate;
}
