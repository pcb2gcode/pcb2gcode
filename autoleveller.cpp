/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2014, 2015 Nicola Corna <nicola@corna.info>
 * 
 * pcb2gcode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pcb2gcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "autoleveller.hpp"

#include <cmath>
#include <limits>

#include <boost/algorithm/string.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#include <boost/format.hpp>
using boost::format;

boost::format silent_format(const string &f_string)
{
    format fmter(f_string);
    fmter.exceptions( boost::io::all_error_bits ^ boost::io::too_many_args_bit ^ boost::io::too_few_args_bit );
    return fmter;
}

autoleveller::autoleveller( const boost::program_options::variables_map &options, uniqueCodes *ocodes,
                            uniqueCodes *globalVars, double quantization_error, double xoffset, double yoffset ) :
    unitconv( options["metric"].as<bool>() ?
              ( options["metricoutput"].as<bool>() ? 1 : 1/25.4 ) :
              ( options["metricoutput"].as<bool>() ? 25.4 : 1 ) ),
    cfactor( options["metricoutput"].as<bool>() ? 25.4 : 1 ),
    XProbeDistRequired( options["al-x"].as<double>() * unitconv ),
    YProbeDistRequired( options["al-y"].as<double>() * unitconv ),
    zwork( str( format("%.5f") % ( options["zwork"].as<double>() * unitconv ) ) ),
    zprobe( str( format("%.3f") % ( options["zsafe"].as<double>() * unitconv ) ) ),
    zsafe( str( format("%.3f") % ( options["zsafe"].as<double>() * unitconv ) ) ),
    zfail( str( format("%.3f") % ( options["metricoutput"].as<bool>() ? FIXED_FAIL_DEPTH_MM : FIXED_FAIL_DEPTH_IN ) ) ),
    feedrate( options["al-probefeed"].as<double>() > 0 ? boost::lexical_cast<string>( options["al-probefeed"].as<double>() * unitconv ) : "" ),
    feedrate2nd( options.count("al-2ndprobefeed") ? boost::lexical_cast<string>( options["al-2ndprobefeed"].as<double>() * unitconv ) : "" ),
    probeOn( boost::replace_all_copy(options["al-probe-on"].as<string>(), "@", "\n") ),
    probeOff( boost::replace_all_copy(options["al-probe-off"].as<string>(), "@", "\n") ),
    software( boost::iequals( options["software"].as<string>(), "linuxcnc" ) ? LINUXCNC :
              boost::iequals( options["software"].as<string>(), "mach3" ) ? MACH3 :
              boost::iequals( options["software"].as<string>(), "mach4" ) ? MACH4 : CUSTOM ),
    quantization_error( quantization_error * cfactor ),
    xoffset( xoffset ),
    yoffset( yoffset ),
    bilinearInterpolationNum( ocodes->getUniqueCode() ),
    g01InterpolatedNum( ocodes->getUniqueCode() ),
    xProbeNum( ocodes->getUniqueCode() ),
    yProbeNum( ocodes->getUniqueCode() ),
    returnVar( boost::lexical_cast<string>( globalVars->getUniqueCode() ) ),
    globalVar0( boost::lexical_cast<string>( globalVars->getUniqueCode() ) ),
    globalVar1( boost::lexical_cast<string>( globalVars->getUniqueCode() ) ),
    globalVar2( boost::lexical_cast<string>( globalVars->getUniqueCode() ) ),
    globalVar3( boost::lexical_cast<string>( globalVars->getUniqueCode() ) ),
    globalVar4( boost::lexical_cast<string>( globalVars->getUniqueCode() ) ),
    globalVar5( boost::lexical_cast<string>( globalVars->getUniqueCode() ) ),
    ocodes( ocodes )
{
    probeCode[LINUXCNC] = "G38.2";
    probeCode[MACH4] = "G31";
    probeCode[MACH3] = "G31";
    probeCode[CUSTOM] = options["al-probecode"].as<string>();
    
    zProbeResultVar[LINUXCNC] = "#5063";
    zProbeResultVar[MACH4] = "#2002";
    zProbeResultVar[MACH3] = "#2002";
    zProbeResultVar[CUSTOM] = "#" + boost::lexical_cast<string>( options["al-probevar"].as<unsigned int>() );
    
    setZZero[LINUXCNC] = "G10 L20 P0 Z0";
    setZZero[MACH4] = "G92 Z0";
    setZZero[MACH3] = "G92 Z0";
    setZZero[CUSTOM] = options["al-setzzero"].as<string>();
    
    callSub2[LINUXCNC] = "o%1$s call [%2$s] [%3$s]\n";
    callSub2[MACH4] = "G65 P%1$s A%2$s B%3$s\n";
    callSub2[MACH3] = "#" + globalVar0 + "=%2$s\n%4$s#" + globalVar1 + "=%3$s\n%4$sM98 P%1$s\n";
    callSub2[CUSTOM] = "";

/*	^ y
 *	|
 *	|	A	B
 *	|
 *	|	C	D
 *	|		    x
 *	+----------->
 *
 * The interpolation macro arguments have this order:
 * 1 - number of the macro
 * 2 - point A
 * 3 - point B
 * 4 - point C
 * 5 - point D
 * 6 - distance between the y coordinate of the point and the y coordinate of C/D
 * 7 - distance between the x coordinate of the point and the x coordinate of A/C
 */

    callInterpolationMacro[LINUXCNC] = "o%1$s call [%2$s] [%3$s] [%4$s] [%5$s] [%6$.5f] [%7$.5f]\n";
    callInterpolationMacro[MACH4] = "G65 P%1$s A%2$s B%3$s C%4$s I%5$s J%6$.5f K%7$.5f\n";
    callInterpolationMacro[MACH3] = "#" + globalVar0 + " = %2$s\n%8$s#" + globalVar1 + " = %3$s\n%8$s#" +
                                    globalVar2 + " = %4$s\n%8$s#" + globalVar3 + " = %5$s\n%8$s#" +
                                    globalVar4 + " = %6$.5f\n%8$s#" + globalVar5 + " = %7$.5f\n%8$sM98 P%1$s\n";
    callInterpolationMacro[CUSTOM] = "#7=[%4$s+[%2$s-%4$s]*%6$.5f]\n#8=[%5$s+[%3$s-%5$s]*%6$.5f]\n#" +
                                     returnVar + "=[#7+[#8-#7]*%7$.5f]\n";
    
    callSubRepeat[LINUXCNC] = "o%3$d repeat [%2%]\n%4$s    o%1% call\n%4$so%3$d endrepeat\n";
    callSubRepeat[MACH4] = "M98 P%1% L%2%\n";
    callSubRepeat[MACH3] = "M98 P%1% L%2%\n";
    callSubRepeat[CUSTOM] = "";
}

string autoleveller::getSoftware()
{
    switch( software )
    {
    case LINUXCNC:  return "LinuxCNC";
    case MACH4:     return "Mach4";
    case MACH3:     return "Mach3";
    case CUSTOM:    return "custom [probe code: " + probeCode[software] + ", probe result variable: "
                               + zProbeResultVar[software] + ", set zero code: " + setZZero[software] + "]";
    }
}

string autoleveller::getVarName( int i, int j )
{
    return '#' + boost::lexical_cast<string>( i * numYPoints + j + 500 );	//getVarName(10,8) returns (numYPoints=10) #180
}

bool autoleveller::prepareWorkarea( vector<shared_ptr<icoords> > &toolpaths )
{
    const std::pair<icoordpair, icoordpair> workarea = computeWorkarea( toolpaths );

    const double workareaLenX = ( workarea.second.first - workarea.first.first ) * cfactor;
    const double workareaLenY = ( workarea.second.second - workarea.first.second ) * cfactor;
    int temp;

    startPointX = workarea.first.first * cfactor;
    startPointY = workarea.first.second * cfactor;

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
            ( software != LINUXCNC && numXPoints * numYPoints > 500 ) )
        return false;
    else
        return true;
}

std::pair<icoordpair, icoordpair> autoleveller::computeWorkarea( vector<shared_ptr<icoords> > &toolpaths )
{
    std::pair<icoordpair, icoordpair> workarea;

    workarea.first.first = std::numeric_limits<double>::infinity();
    workarea.first.second = std::numeric_limits<double>::infinity();
    workarea.second.first = -std::numeric_limits<double>::infinity();
    workarea.second.second = -std::numeric_limits<double>::infinity();

    BOOST_FOREACH( shared_ptr<icoords> path, toolpaths )
    {
        workarea.first.first = std::min(workarea.first.first, std::min_element( path->begin(), path->end(),
                                        boost::bind(&icoordpair::first, _1) < boost::bind(&icoordpair::first, _2) )->first );

        workarea.first.second = std::min(workarea.first.second, std::min_element( path->begin(), path->end(),
                                         boost::bind(&icoordpair::second, _1) < boost::bind(&icoordpair::second, _2) )->second );

        workarea.second.first = std::max(workarea.second.first, std::max_element( path->begin(), path->end(),
                                         boost::bind(&icoordpair::first, _1) < boost::bind(&icoordpair::first, _2) )->first );

        workarea.second.second = std::max(workarea.second.second, std::max_element( path->begin(), path->end(),
                                          boost::bind(&icoordpair::second, _1) < boost::bind(&icoordpair::second, _2) )->second );
    }

    workarea.first.first -= xoffset + quantization_error;
    workarea.first.second -= yoffset + quantization_error;
    workarea.second.first -= xoffset - quantization_error;
    workarea.second.second -= yoffset - quantization_error;

    return workarea;
}

void autoleveller::header( std::ofstream &of )
{
    const char *logFileOpenAndComment[] =
    {
        "(PROBEOPEN RawProbeLog.txt) ( Record all probes in RawProbeLog.txt )",
        "M40 (Begins a probe log file, when the window appears, enter a name for the log file such as \"RawProbeLog.txt\")",
        "M40 (Begins a probe log file, when the window appears, enter a name for the log file such as \"RawProbeLog.txt\")",
        "( No probe log function available in custom mode )"
    };
    const char *logFileClose[] = { "(PROBECLOSE)" , "M41", "M41", "( No probe log function available in custom mode )" };
    int i;
    int j = 1;
    int incr_decr = 1;

    if( software == LINUXCNC )
        footerNoIf( of );

    if( !feedrate.empty() )
    {
        of << probeOn << endl;
        of << "G0 Z" << zsafe << " ( Move Z to safe height )"<< endl;
        of << "G0 X" << startPointX << " Y" << startPointY << " ( Move XY to start point )" << endl;
        of << "G0 Z" << zprobe << " ( Move Z to probe height )" << endl;
        of << logFileOpenAndComment[software] << endl;
        of << probeCode[software] << " Z" << zfail << " F" << feedrate << " ( Z-probe )" << endl;
        of << "#500 = 0 ( Probe point [0, 0] is our reference )" << endl;
        of << setZZero[software] << " ( Set the current Z as zero-value )" << endl;
        of << endl;
        of << "( We now start the real probing: move the Z axis to the probing height, move to )" << endl;
        of << "( the probing XY position, probe it and save the result, parameter " << zProbeResultVar[software] << ", )" << endl;
        of << "( in a numbered parameter; we will make " << numXPoints << " probes on the X-axis and )" << endl;
        of << "( " << numYPoints << " probes on the Y-axis, for a grand total of " << numXPoints * numYPoints << " probes )" << endl;
        of << endl;

        if( software != CUSTOM )
        {
            of << "#" << globalVar0 << " = 0 ( X iterator )" << endl;
            of << "#" << globalVar1 << " = 1 ( Y iterator )" << endl;
            of << "#" << globalVar2 << " = 1 ( UP or DOWN increment )" << endl;
            of << "#" << globalVar3 << " = " << numYPoints - 1 << " ( number of Y points; the 1st Y row can be done one time less )" << endl;
            of << silent_format( callSubRepeat[software] ) % xProbeNum % numXPoints % ocodes->getUniqueCode();
        }
        else
        {
            for( i = 0; i < numXPoints; i++ )
            {
                while( j >= 0 && j <= numYPoints - 1 )
                {
                    of << "G0 Z" << zprobe << endl;
                    of << "X" << i * XProbeDist + startPointX << " Y" << j * YProbeDist + startPointY << endl;
                    of << probeCode[software] << " Z" << zfail << " F" << feedrate << endl;
                    of << getVarName(i, j) << "=" << zProbeResultVar[software] << endl;
                    j += incr_decr;
                }
                incr_decr = -incr_decr;
                j += incr_decr;
            }
        }
    }

    if( !feedrate2nd.empty() )
    {
        of << endl;
        of << "T2" << endl;
        of << "(MSG, Insert the mill tool)" << endl;
        of << "M0 (Temporary machine stop.)" << endl;
        of << "G0 Z[" << zsafe << " + " << 0.2 * cfactor << "] ( Move Z to safe height )"<< endl;
        of << "G0 X" << startPointX << " Y" << startPointY << " ( Move XY to start point )" << endl;
        of << "G0 Z[" << zprobe << " + " << 0.2 * cfactor << "] ( Move Z to probe height )" << endl;
        of << probeCode[software] << " Z[" << zfail << " - " << 0.2 * cfactor << "] F" << feedrate2nd << " ( Probe )" << endl;
        of << setZZero[software] << " ( Set the current Z as zero-value )" << endl;
    }

    of << endl;
    of << "G0 Z" << zsafe << " ( Move Z to safe height )"<< endl;
    of << logFileClose[software] << " ( Close the probe log file )" << endl;
    of << "( Probing has ended, each Z-coordinate will be corrected with a bilinear interpolation )" << endl;
    of << probeOff << endl;
    of << endl;
}

void autoleveller::footerNoIf( std::ofstream &of )
{
    const char *startSub[] = { "o%1$d sub", "O%1$d", "O%1$d", "" };
    const char *endSub[] = { "o%1$d endsub", "M99", "M99", "" };
    const char *callSubSimple2[] = { "o%1$s call [%2$s] [%3$s]\n", "G65 P%1$s A%2$s B%3$s\n", "M98 P%1$s\n", "" };
    const char *var1[] = { "1", "1", globalVar0.c_str(), globalVar0.c_str() };
    const char *var2[] = { "2", "2", globalVar1.c_str(), globalVar1.c_str() };
    const char *var3[] = { "3", "3", globalVar2.c_str(), globalVar2.c_str() };
    const char *var4[] = { "4", "4", globalVar3.c_str(), globalVar3.c_str() };
    const char *var5[] = { "5", "5", globalVar4.c_str(), globalVar4.c_str() };
    const char *var6[] = { "6", "6", globalVar5.c_str(), globalVar5.c_str() };
    const unsigned int correctionFactorNum = ocodes->getUniqueCode();

    if( software != CUSTOM )
    {
        of << format(startSub[software]) % bilinearInterpolationNum << " ( Bilinear interpolation macro )" << endl;
        of << "    #7 = [ #" << var3[software] << " + [ #" << var1[software] << " - #" << var3[software]
           << " ] * #" << var5[software] << " ] ( Linear interpolation of the x-min elements )" << endl;
        of << "    #8 = [ #" << var4[software] << " + [ #" << var2[software] << " - #" << var4[software]
           << " ] * #" << var5[software] << " ] ( Linear interpolation of the x-max elements )" << endl;
        of << "    #" << returnVar << " = [ #7 + [ #8 - #7 ] * #" << var6[software] << " ] ( Linear interpolation of previously interpolated points )" << endl;
        of << silent_format(endSub[software]) % bilinearInterpolationNum << endl;
        of << endl;
        of << format(startSub[software]) % correctionFactorNum << " ( Z-correction subroutine )" << endl;
        of << "    #3 = [ FIX[ [ #" << var1[software] << " - " << startPointX << " ] / " << XProbeDist << " ] ] ( Lower left point X index )" << endl;
        of << "    #4 = [ FIX[ [ #" << var2[software] << " - " << startPointY << " ] / " << YProbeDist << " ] ] ( Lower left point Y index )" << endl;
        if( software == MACH3 )
        {
            of << "    #11 = #" << globalVar0 << " ( Save global parameter " << globalVar0 << " )" << endl;
            of << "    #12 = #" << globalVar1 << " ( and " << globalVar1 << " )" << endl;
        }
        of << "    #5 = [ #3 * " << numYPoints << " + [ #4 + 1 ] + 500 ] ( Upper left point parameter number )" << endl;
        of << "    #6 = [ [ #3 + 1 ] *" << numYPoints << " + [ #4 + 1 ] + 500 ] ( Upper right point parameter number )" << endl;
        of << "    #7 = [ #3 * " << numYPoints << " + #4 + 500 ] ( Lower left point parameter number )" << endl;
        of << "    #8 = [ [ #3 + 1 ] * " << numYPoints << " + #4 + 500 ] ( Lower right point parameter number )" << endl;
        of << "    #9 = [ [ #" << var2[software] << " - " << startPointY << " - #4 * " << YProbeDist << " ] / " << YProbeDist << " ] "
           "( Distance between the point and the left border of the rectangle, normalized to 1 )" << endl;
        of << "    #10 = [ [ #" << var1[software] << " - " << startPointX << " - #3 * " << XProbeDist << " ] / " << XProbeDist << " ] "
           "( Distance between the point and the down border of the rectangle, normalized to 1 ) " << endl;
        of << "    " << str( silent_format( callInterpolationMacro[software] ) % bilinearInterpolationNum %
                             "##5" % "##6" % "##7" % "##8" % "#9" % "#10" % "    " );
        if( software == MACH3 )
        {
            of << "    #" << globalVar0 << " = #11 ( Restore global parameter " << globalVar0 << " )" << endl;
            of << "    #" << globalVar1 << " = #12 ( and " << globalVar1 << " )" << endl;
        }
        of << silent_format(endSub[software]) % correctionFactorNum << endl;
        of << endl;
        of << format(startSub[software]) % g01InterpolatedNum << " ( G01 with Z-correction subroutine )" << endl;
        of << "    " << silent_format(callSubSimple2[software]) % correctionFactorNum %
           ( string("#") + var1[software] ) % ( string("#") + var2[software] );
        of << "    G01 X#" << var1[software] << " Y#" << var2[software] << " Z[" + zwork + "+#" << returnVar << "]" << endl;
        of << silent_format(endSub[software]) % g01InterpolatedNum << endl;
        of << endl;
        of << format( startSub[software] ) % yProbeNum << " ( Y probe subroutine )" << endl;
        of << "    G0 Z" << zprobe << " ( Move to probe height )" << endl;
        of << "    X[#" << globalVar0 << " * " << XProbeDist << " + " << startPointX << "] Y[#" << globalVar1 << " * " << YProbeDist << " + " << startPointY << "] "
           " ( Move to the current probe point )" << endl;
        of << "    " << probeCode[software] << " Z" << zfail << " F" << feedrate << " ( Probe it )" << endl;
        of << "    #[#" << globalVar0 << " * " << numYPoints << " + #" << globalVar1 << " + 500] = " << zProbeResultVar[software] <<
           " ( Save the probe in the correct parameter )" << endl;
        of << "    #" << globalVar1 << " = [#" << globalVar1 << " + #" << globalVar2 << "] ( Increment/decrement by 1 the Y counter )" << endl;
        of << silent_format( endSub[software] ) % yProbeNum << endl;
        of << endl;
        of << format( startSub[software] ) % xProbeNum << " ( X probe subroutine )" << endl;
        of << "    " << silent_format( callSubRepeat[software] ) % yProbeNum % ( "#" + globalVar3 ) % ocodes->getUniqueCode() % "    ";
        of << "    #" << globalVar3 << " = " << numYPoints << endl;
        of << "    #" << globalVar2 << " = [0 - #" << globalVar2 << "]" << endl;
        of << "    #" << globalVar1 << " = [#" << globalVar1 << " + #" << globalVar2 << ']' << endl;
        of << "    #" << globalVar0 << " = [#" << globalVar0 << " + 1] ( Increment by 1 the X counter )" << endl;
        of << silent_format( endSub[software] ) % xProbeNum << endl;
        of << endl;
    }
}

string autoleveller::interpolatePoint ( icoordpair point )
{
    int xminindex;
    int yminindex;
    double x_minus_x0_rel;
    double y_minus_y0_rel;

    xminindex = floor ( ( point.first - startPointX ) / XProbeDist );
    yminindex = floor ( ( point.second - startPointY ) / YProbeDist );
    x_minus_x0_rel = ( point.first - startPointX - xminindex * XProbeDist ) / XProbeDist;
    y_minus_y0_rel = ( point.second - startPointY - yminindex * YProbeDist ) / YProbeDist;

    return str( silent_format( callInterpolationMacro[software] ) % bilinearInterpolationNum %
                getVarName( xminindex, yminindex + 1 ) %
                getVarName( xminindex + 1, yminindex + 1 ) %
                getVarName( xminindex, yminindex ) %
                getVarName( xminindex + 1, yminindex ) %
                y_minus_y0_rel % x_minus_x0_rel );
}

string autoleveller::addChainPoint ( icoordpair point )
{
    string outputStr;
    icoords subsegments;
    icoords::const_iterator i;

    subsegments = splitSegment( point, numOfSubsegments( point ) );

    if( software == LINUXCNC || software == MACH4 || software == MACH3 )
        for( i = subsegments.begin(); i != subsegments.end(); i++ )
            outputStr += str( silent_format( callSub2[software] ) % g01InterpolatedNum % i->first % i->second );
    else
        for( i = subsegments.begin(); i != subsegments.end(); i++ )
        {
            outputStr += interpolatePoint( *i );
            outputStr += str( format( "X%1$.5f Y%2$.5f Z[%3$s+#" + returnVar + "]\n" ) % i->first % i->second % zwork );
        }

    lastPoint = point;
    return outputStr;
}

string autoleveller::g01Corrected ( icoordpair point )
{
    if( software == LINUXCNC || software == MACH4 || software == MACH3 )
        return str( silent_format( callSub2[software] ) % g01InterpolatedNum % point.first % point.second );
    else
        return interpolatePoint( point ) + "G01 Z[" + zwork + "+#" + returnVar + "]\n";
}

unsigned int autoleveller::numOfSubsegments ( icoordpair point )
{

    if( abs( lastPoint.first - point.first ) <= quantization_error )	//The two points are X-aligned
        return ceil( boost::geometry::distance( lastPoint, point ) / YProbeDist );

    else if( abs( lastPoint.second - point.second ) <= quantization_error )	//The two points are Y-aligned
        return ceil( boost::geometry::distance( lastPoint, point ) / XProbeDist );

    else	//The two points aren't aligned
        return ceil( boost::geometry::distance( lastPoint, point ) / averageProbeDist );
}

icoords autoleveller::splitSegment ( const icoordpair point, const unsigned int n )
{
    icoords splittedSegment;

    for( unsigned int i = 1; i <= n; i++ )
        splittedSegment.push_back( icoordpair( lastPoint.first + ( point.first - lastPoint.first ) * i / n,
                                               lastPoint.second + ( point.second - lastPoint.second ) * i / n ) );

    return splittedSegment;

}
