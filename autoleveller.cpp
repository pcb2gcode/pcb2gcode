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

#include <cmath>

#include <boost/algorithm/string/replace.hpp>
using boost::replace_all;

#include <boost/format.hpp>
using boost::format;

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

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

const char *autoleveller::callSub2[] = { "o%1$s call [%2$s] [%3$s]\n",
                                         "G65 P%1$s A%2$s B%3$s\n",
                                         "#" GLOB_VAR_0 "=%2$s\n%4$s#" GLOB_VAR_1 "=%3$s\n%4$sM98 P%1$s\n",
                                         "#" GLOB_VAR_0 "=%2$s\n%4$s#" GLOB_VAR_1 "=%3$s\n%4$sM98 O%1$s\n", };
                                        
const char *autoleveller::callInterpolationMacro[] = { "o%1$s call [%2$s] [%3$s] [%4$s] [%5$s] [%6$.5f] [%7$.5f]\n",
                                                       "G65 P%1$s A%2$s B%3$s C%4$s I%5$s J%6$.5f K%7$.5f\n",
                                                       "#" GLOB_VAR_0 "=%2$s\n%8$s#" GLOB_VAR_1 "=%3$s\n%8$s#" GLOB_VAR_2 "=%4$s\n%8$s#" GLOB_VAR_3 "=%5$s\n%8$s#" GLOB_VAR_4 "=%6$.5f\n%8$s#" GLOB_VAR_5 "=%7$.5f\n%8$sM98 P%1$s\n",
                                                       "#" GLOB_VAR_0 "=%2$s\n%8$s#" GLOB_VAR_1 "=%3$s\n%8$s#" GLOB_VAR_2 "=%4$s\n%8$s#" GLOB_VAR_3 "=%5$s\n%8$s#" GLOB_VAR_4 "=%6$.5f\n%8$s#" GLOB_VAR_5 "=%7$.5f\n%8$sM98 O%1$s\n" };

boost::format silent_format(const string &f_string) {
    format fmter(f_string);
    fmter.exceptions( boost::io::all_error_bits ^ boost::io::too_many_args_bit ^ boost::io::too_few_args_bit );
    return fmter;
}

/******************************************************************************/
/*	
 *  Constructor, throws an exception if the number of required variables (probe
 *  points) exceeds the maximum number of variables
 */
/******************************************************************************/
autoleveller::autoleveller(double XProbeDist, double YProbeDist, double zwork, Software software) :
 XProbeDistRequired( XProbeDist ),
 YProbeDistRequired( YProbeDist ),
 zwork( str( format("%.3f") % zwork ) ),
 software( software )
{

}

/******************************************************************************/
/*
 */
/******************************************************************************/
string autoleveller::getSoftware() {
    switch( software ) {
        case LINUXCNC:  return "LinuxCNC";
        case MACH4:     return "Mach4";
        case MACH3:     return "Mach3";
        case TURBOCNC:  return "TurboCNC";
    }
}

/******************************************************************************/
/*
 *  Returns the ij-th variable name (based on software)
 */
/******************************************************************************/
string autoleveller::getVarName( int i, int j ) {
	std::stringstream ss;

    ss << '#' << i * numYPoints + j + 500;	//getVarName(10,8) returns (numYPoints=10) #180

	return ss.str();
}

/******************************************************************************/
/*
 *  Generate the gcode probe header
 */
/******************************************************************************/
void autoleveller::probeHeader( std::ofstream &of, std::pair<icoordpair, icoordpair> workarea, double zprobe, double zsafe, double zfail, int feedrate, std::string probeOn, std::string probeOff ) {
	const char *probeCode[] = { "G38.2", "G31", "G31", "G31" };
	const char *setZero[] = { "G10 L20 P0 Z0", "G92 Z0", "G92 Z0", "G92 Z0" };
	const char *zProbeResultVar[] = { "#5063", "#2002", "#2002", "#2002" };
	const char *logFileOpenAndComment[] =
	 { "(PROBEOPEN RawProbeLog.txt) ( Record all probes in RawProbeLog.txt )",
	   "M40 (Begins a probe log file, when the window appears, enter a name for the log file such as \"RawProbeLog.txt\")",
	   "M40 (Begins a probe log file, when the window appears, enter a name for the log file such as \"RawProbeLog.txt\")",
	   "( No probe log function available in turboCNC )" };
	const char *logFileClose[] = { "(PROBECLOSE)" , "M41", "M41", "( No probe log function available in turboCNC )" };
	const char *startSub[] = { "o%1$d sub", "O%1$d", "O%1$d", "O%1$d" };
	const char *endSub[] = { "o%1$d endsub", "M99", "M99", "M99" };
    const char *callSubRepeat[] = { "o%3$d repeat [%2%]\n%5$s    o%1% call\n%5$so%3$d endrepeat\n", "M98 P%1% L%2%\n", "M98 P%1% L%2%\n", "#1=0\n%5$sN%3$d IF #1 GE %2% M97 N%4$d\n%5$s    M98 O%1%\n%5$s    #1=[#1+1]\n%5$s    M97 N%3$d\n%5$sN%4$d\n" };
    const char *callSub[] = { "o%1% call\n", "M98 P%1%", "M98 P%1%", "M98 O%1%" };
    const char *callSubSub2[] = { "o%1$s call [%2$s] [%3$s]\n", "G65 P%1$s A%2$s B%3$s\n", "M98 P%1$s\n", "M98 O%1$s\n" };
    const char *var1[] = { "1", "1", GLOB_VAR_0, GLOB_VAR_0 };
    const char *var2[] = { "2", "2", GLOB_VAR_1, GLOB_VAR_1 };
    const char *var3[] = { "3", "3", GLOB_VAR_2, GLOB_VAR_2 };
    const char *var4[] = { "4", "4", GLOB_VAR_3, GLOB_VAR_3 };
    const char *var5[] = { "5", "5", GLOB_VAR_4, GLOB_VAR_4 };
    const char *var6[] = { "6", "6", GLOB_VAR_5, GLOB_VAR_5 };
    int temp;

	replace_all(probeOn, "@", "\n");
	replace_all(probeOff, "@", "\n");

    workareaLenX = workarea.second.first - workarea.first.first;
    workareaLenY = workarea.second.second - workarea.first.second;
    startPointX = workarea.first.first;
    startPointY = workarea.first.second;
    
    temp = round ( workareaLenX / XProbeDistRequired );    //We need at least 2 probe points
    if( temp > 1 )
        numXPoints = temp + 1;
    else
        numXPoints = 2;

    temp = round ( workareaLenY / YProbeDistRequired );    //We need at least 2 probe points
    if( temp > 1 )
        numYPoints = temp + 1;
    else
        numYPoints = 2;

    XProbeDist = workareaLenX / ( numXPoints - 1 );
    YProbeDist = workareaLenY / ( numYPoints - 1 );
    averageProbeDist = ( XProbeDist + YProbeDist ) / 2;
    
    if( ( software == LINUXCNC && numXPoints * numYPoints > 4501 ) ||
          software != LINUXCNC && numXPoints * numYPoints > 500 )
        throw autoleveller_exception();

    if( software == LINUXCNC || software == MACH4 || software == MACH3 ) {
   	    of << format(startSub[software]) % BILINEAR_INTERPOLATION_MACRO_NUMBER << " ( Bilinear interpolation macro )" << endl;
        of << "    #7=[#" << var3[software] << "+[#" << var1[software] << "-#" << var3[software]
           << "]*#" << var5[software] << "] ( Linear interpolation of the x-min elements )" << endl;
        of << "    #8=[#" << var4[software] << "+[#" << var2[software] << "-#" << var4[software]
           << "]*#" << var5[software] << "] ( Linear interpolation of the x-max elements )" << endl;
        of << "    #" RESULT_VAR "=[#7+[#8-#7]*#" << var6[software] << "] ( Linear interpolation of previously interpolated points )" << endl;
        of << silent_format(endSub[software]) % BILINEAR_INTERPOLATION_MACRO_NUMBER << endl;
        of << endl;
        of << format(startSub[software]) % CORRECTION_FACTOR_SUB_NUMBER << " ( Z-correction subroutine )" << endl;
        of << "    #3=[FIX[[#" << var1[software] << '-' << startPointX << "]/" << XProbeDist << "]]" << endl;
        of << "    #4=[FIX[[#" << var2[software] << '-' << startPointY << "]/" << YProbeDist << "]]" << endl;
        of << "    #5=[#3*" << numYPoints << "+[#4+1]+500]" << endl;
        of << "    #6=[[#3+1]*" << numYPoints << "+[#4+1]+500]" << endl;
        of << "    #7=[#3*" << numYPoints << "+#4+500]" << endl;
        of << "    #8=[[#3+1]*" << numYPoints << "+#4+500]" << endl;
        of << "    #9=[[#" << var2[software] << '-' << startPointY << "-#4*" << YProbeDist << "]/" << YProbeDist << ']' << endl;
        of << "    #10=[[#" << var1[software] << '-' << startPointX << "-#3*" << XProbeDist << "]/" << XProbeDist << ']' << endl;
        of << "    " << str( silent_format( callInterpolationMacro[software] ) % BILINEAR_INTERPOLATION_MACRO_NUMBER %
                                                                  "##5" % "##6" % "##7" % "##8" % "#9" % "#10" % "    " );
        of << silent_format(endSub[software]) % CORRECTION_FACTOR_SUB_NUMBER << endl;
        of << endl;
    	of << format(startSub[software]) % G01_INTERPOLATED_MACRO_NUMBER << " ( G01 with Z-correction subroutine )" << endl;
    	of << "    " << silent_format(callSubSub2[software]) % CORRECTION_FACTOR_SUB_NUMBER % ( string("#") + var1[software] ) % ( string("#") + var2[software] );
        of << "    G01 X#" << var1[software] << " Y#" << var2[software] << " Z[" + zwork + "+#" RESULT_VAR "]" << endl;
        of << silent_format(endSub[software]) % G01_INTERPOLATED_MACRO_NUMBER << endl;
       	of << endl;
	}
	
    of << format( startSub[software] ) % YPROBE_SUB_NUMBER << " ( Y probe subroutine )" << endl;
    of << "    G0 Z" << zprobe << endl;
    of << "    X[#" GLOB_VAR_0 " * " << XProbeDist << " + " << startPointX << "] Y[#" GLOB_VAR_1 " * " << YProbeDist << " + " << startPointY << ']' << endl;
    of << "    " << probeCode[software] << " Z" << zfail << " F" << feedrate << endl;
    of << "    #[#" GLOB_VAR_0 " * " << numYPoints << " + #" GLOB_VAR_1 " + 500] = " << zProbeResultVar[software] << endl;
    of << "    #" GLOB_VAR_1 " = [#" GLOB_VAR_1 " + #" << GLOB_VAR_2 << ']' << endl;
    of << silent_format( endSub[software] ) % YPROBE_SUB_NUMBER << endl;
    of << endl;
    of << format( startSub[software] ) % XPROBE_SUB_NUMBER << " ( X probe subroutine )" << endl;
    of << "    " << silent_format( callSubRepeat[software] ) % YPROBE_SUB_NUMBER % "#" GLOB_VAR_3 % REPEAT_CODE % ( REPEAT_CODE + 10 ) % "    ";
    of << "    #" GLOB_VAR_3 " = " << numYPoints << endl;
    of << "    #" GLOB_VAR_2 " = [-#" GLOB_VAR_2 "]" << endl;
    of << "    #" GLOB_VAR_1 " = [#" GLOB_VAR_1 " + #" << GLOB_VAR_2 << ']' << endl;
    of << "    #" GLOB_VAR_0 " = [#" GLOB_VAR_0 " + 1]" << endl;
    of << silent_format( endSub[software] ) % XPROBE_SUB_NUMBER << endl;
    of << endl;
    of << probeOn << endl;
    of << "G0 Z" << zsafe << " ( Move Z to safe height )"<< endl;
    of << "G0 X" << startPointX << " Y" << startPointY << " ( Move XY to start point )" << endl;
    of << "G0 Z" << zprobe << " ( Move Z to probe height )" << endl;
    of << logFileOpenAndComment[software] << endl;
    of << probeCode[software] << " Z" << zfail << " F" << feedrate << " ( Z-probe )" << endl;
    of << "#500 = 0 ( Probe point [0, 0] is our reference )" << endl;
    of << setZero[software] << " ( Set the current Z as zero-value )" << endl;
    of << endl;	
    of << "( We now start the real probing: move the Z axis to the probing height, move to )" << endl;
	of << "( the probing XY position, probe it and save the result, parameter " << zProbeResultVar[software] << ", )" << endl;
	of << "( in a numbered parameter; we will make " << numXPoints << " probes on the X-axis and )" << endl;
	of << "( " << numYPoints << " probes on the Y-axis, for a grand total of " << numXPoints * numYPoints << " probes )" << endl;
    of << endl;
    of << "#" GLOB_VAR_0 " = 0 ( X iterator )" << endl;
    of << "#" GLOB_VAR_1 " = 1 ( Y iterator )" << endl;
    of << "#" GLOB_VAR_2 " = 1 ( UP or DOWN increment )" << endl;
    of << "#" GLOB_VAR_3 " = " << numYPoints - 1 << " ( number of Y points; the 1st Y row can be done one time less )" << endl;
    of << silent_format( callSubRepeat[software] ) % XPROBE_SUB_NUMBER % numXPoints % ( REPEAT_CODE + 100 ) % ( REPEAT_CODE + 110 );
    of << endl;
	of << "G0 Z" << zsafe << " ( Move Z to safe height )"<< endl;
	of << logFileClose[software] << " ( Close the probe log file )" << endl;
	of << "( Probing has ended, each Z-coordinate will be corrected with a bilinear interpolation )" << endl;
	of << probeOff << endl;
	of << endl;
}

/******************************************************************************/
/*
 *  Find the correct rectangle and write the bilinear interpolation of the point
 */
/******************************************************************************/
string autoleveller::interpolatePoint ( icoordpair point ) {
	int xminindex;
	int yminindex;
	double x_minus_x0_rel;
	double y_minus_y0_rel;
	
	xminindex = floor ( ( point.first - startPointX ) / XProbeDist );
	yminindex = floor ( ( point.second - startPointY ) / YProbeDist );
	x_minus_x0_rel = ( point.first - startPointX - xminindex * XProbeDist ) / XProbeDist;
	y_minus_y0_rel = ( point.second - startPointY - yminindex * YProbeDist ) / YProbeDist;
	
	return str( silent_format( callInterpolationMacro[software] ) % BILINEAR_INTERPOLATION_MACRO_NUMBER %
													  getVarName( xminindex, yminindex + 1 ) %
												      getVarName( xminindex + 1, yminindex + 1 ) %
												      getVarName( xminindex, yminindex ) %
												      getVarName( xminindex + 1, yminindex ) %
												      y_minus_y0_rel % x_minus_x0_rel );
}

/******************************************************************************/
/*
 *  Interpolate a point and eventually add intermediate points
 */
/******************************************************************************/
string autoleveller::addChainPoint ( icoordpair point ) {
	string outputStr;
	icoords subsegments;
	icoords::const_iterator i;

	subsegments = splitSegment( point, numOfSubsegments( point ) );

    if( software == LINUXCNC || software == MACH4 || software == MACH3 )
	    for( i = subsegments.begin(); i != subsegments.end(); i++ )
            outputStr += str( silent_format( callSub2[software] ) % G01_INTERPOLATED_MACRO_NUMBER % i->first % i->second );
    else
	    for( i = subsegments.begin(); i != subsegments.end(); i++ ) {
		    outputStr += interpolatePoint( *i );
		    outputStr += str( format( "X%1$.5f Y%2$.5f Z[%3$s+#" RESULT_VAR "]\n" ) % i->first % i->second % zwork );
	    }

	lastPoint = point;
	return outputStr;
}

string autoleveller::g01Corrected ( icoordpair point ) {
	if( software == LINUXCNC || software == MACH4 || software == MACH3 )
	    return str( silent_format( callSub2[software] ) % G01_INTERPOLATED_MACRO_NUMBER % point.first % point.second );
	else
    	return interpolatePoint( point ) + "G01 Z[" + zwork + "+#" RESULT_VAR "]\n";
}

double autoleveller::pointDistance ( icoordpair p0, icoordpair p1 ) {
	double x1_x0 = p1.first - p0.first;
	double y1_y0 = p1.second - p0.second;
	
	return sqrt( x1_x0 * x1_x0 + y1_y0 * y1_y0 );
}

unsigned int autoleveller::numOfSubsegments ( icoordpair point ) {

	if( abs( lastPoint.first - point.first ) <= 0.001 )	//The two points are X-aligned
		return ceil( pointDistance( lastPoint, point ) / YProbeDist );
		
	else if( abs( lastPoint.second - point.second ) <= 0.001 )	//The two points are Y-aligned
		return ceil( pointDistance( lastPoint, point ) / XProbeDist );
		
	else	//The two points aren't aligned
		return ceil( pointDistance( lastPoint, point ) / averageProbeDist );
}

icoords autoleveller::splitSegment ( const icoordpair point, const unsigned int n ) {
	icoords splittedSegment;
	
	for( unsigned int i = 1; i <= n; i++ )
		splittedSegment.push_back( icoordpair( lastPoint.first + ( point.first - lastPoint.first ) * i / n,
											   lastPoint.second + ( point.second - lastPoint.second ) * i / n ) );
	
	return splittedSegment;
	
}
