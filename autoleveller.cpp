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

#include <boost/format.hpp>
#include <memory>
#include <vector>
using boost::format;
using std::shared_ptr;
using std::vector;
using std::endl;
using std::to_string;
using std::string;

#include <utility>
using std::pair;

#include "units.hpp"

#include "bg_operators.hpp"

const string autoleveller::callSubRepeat[] = {
 "o%3$d repeat [%2%]\n%4$s    o%1% call\n%4$so%3$d endrepeat\n",
 "M98 P%1% L%2%\n",
 "M98 P%1% L%2%\n" };

const string autoleveller::probeCode[] = { "G38.2", "G31", "G31" };
const string autoleveller::zProbeResultVar[] = { "#5063", "#5063", "#2002" };
const string autoleveller::setZZero[] = { "G10 L20 P0 Z0", "G92 Z0", "G92 Z0" };

boost::format silent_format(const string &f_string)
{
    format fmter(f_string);
    fmter.exceptions( boost::io::all_error_bits ^ boost::io::too_many_args_bit ^ boost::io::too_few_args_bit );
    return fmter;
}

autoleveller::autoleveller( const boost::program_options::variables_map &options, uniqueCodes *ocodes,
                            uniqueCodes *globalVars, double xoffset, double yoffset,
                            const struct Tiling::TileInfo tileInfo ) :
    input_unitconv( options["metric"].as<bool>() ? 1.0/25.4 : 1),
    output_unitconv( options["metricoutput"].as<bool>() ? 25.4 : 1),
    cfactor( options["metricoutput"].as<bool>() ? 25.4 : 1 ),
    probeCodeCustom( options["al-probecode"].as<string>() ),
    zProbeResultVarCustom( "#" + to_string(options["al-probevar"].as<unsigned int>()) ),
    setZZeroCustom( options["al-setzzero"].as<string>() ),
    XProbeDistRequired( options["al-x"].as<Length>().asInch(input_unitconv) * output_unitconv ),
    YProbeDistRequired( options["al-y"].as<Length>().asInch(input_unitconv) * output_unitconv ),
    zprobe( str( format("%.3f") % ( options["zsafe"].as<Length>().asInch(input_unitconv) * output_unitconv ) ) ),
    zsafe( str( format("%.3f") % ( options["zsafe"].as<Length>().asInch(input_unitconv) * output_unitconv) ) ),
    zfail( str( format("%.3f") % ( options["metricoutput"].as<bool>() ? FIXED_FAIL_DEPTH_MM : FIXED_FAIL_DEPTH_IN ) ) ),
    feedrate( std::to_string( options["al-probefeed"].as<Velocity>().asInchPerMinute(input_unitconv) * output_unitconv ) ),
    probeOn( boost::replace_all_copy(options["al-probe-on"].as<string>(), "@", "\n") ),
    probeOff( boost::replace_all_copy(options["al-probe-off"].as<string>(), "@", "\n") ),
    software( options["software"].as<Software::Software>() ),
    xoffset( xoffset ),
    yoffset( yoffset ),
    g01InterpolatedNum( ocodes->getUniqueCode() ),
    yProbeNum( ocodes->getUniqueCode() ),
    xProbeNum( ocodes->getUniqueCode() ),
    returnVar( to_string(globalVars->getUniqueCode()) ),
    globalVar0( to_string(globalVars->getUniqueCode()) ),
    globalVar1( to_string(globalVars->getUniqueCode()) ),
    globalVar2( to_string(globalVars->getUniqueCode()) ),
    globalVar3( to_string(globalVars->getUniqueCode()) ),
    globalVar4( to_string(globalVars->getUniqueCode()) ),
    globalVar5( to_string(globalVars->getUniqueCode()) ),
    tileInfo( tileInfo ),
    initialXOffsetVar( globalVars->getUniqueCode() ),
    initialYOffsetVar( globalVars->getUniqueCode() ),
    ocodes( ocodes )
{
    callSub2[Software::LINUXCNC] = "o%1$s call [%2$s] [%3$s] [%4$s]\n";
    callSub2[Software::MACH4] = "G65 P%1$s A%2$s B%3$s C%4$s\n";
    callSub2[Software::MACH3] = "#" + globalVar0 + "=%2$s\n%5$s#" + globalVar1 + "=%3$s\n%6$s#" + globalVar2 + "=%4$s\n%7$sM98 P%1$s\n";
}

string autoleveller::getVarName( int i, int j )
{
    return '#' + to_string(i * numYPoints + j + 500);	//getVarName(10,8) returns (numYPoints=10) #180
}

box_type_fp computeWorkarea(const vector<pair<coordinate_type_fp, multi_linestring_type_fp>>& toolpaths) {
  box_type_fp bounding_box = boost::geometry::make_inverse<box_type_fp>();

  for (const auto& toolpath : toolpaths) {
    for (const auto& linestring : toolpath.second) {
      boost::geometry::expand(bounding_box,
                              boost::geometry::return_envelope<box_type_fp>(linestring));
    }
  }

  return bounding_box;
}

void autoleveller::prepareWorkarea(const vector<pair<coordinate_type_fp, multi_linestring_type_fp>>& toolpaths) {
    box_type_fp workarea;
    double workareaLenX;
    double workareaLenY;

    workarea = computeWorkarea(toolpaths);
    workarea.min_corner().x(workarea.min_corner().x() - xoffset);
    workarea.min_corner().y(workarea.min_corner().y() - yoffset);
    workarea.max_corner().x(workarea.max_corner().x() - xoffset);
    workarea.max_corner().y(workarea.max_corner().y() - yoffset);

    workareaLenX = ( workarea.max_corner().x() - workarea.min_corner().x() ) * cfactor + 
                   tileInfo.boardWidth * cfactor * ( tileInfo.tileX - 1 );
    workareaLenY = ( workarea.max_corner().y() - workarea.min_corner().y() ) * cfactor +
                   tileInfo.boardHeight * cfactor * ( tileInfo.tileY - 1 );

    startPointX = workarea.min_corner().x() * cfactor;
    startPointY = workarea.min_corner().y() * cfactor;

    numXPoints = ceil(workareaLenX / XProbeDistRequired) + 1;
    numXPoints = std::max(2U, numXPoints); //We need at least 2 probe points

    numYPoints = ceil(workareaLenY / YProbeDistRequired) + 1;    //We need at least 2 probe points
    numYPoints = std::max(2U, numYPoints); //We need at least 2 probe points

    XProbeDist = workareaLenX / ( numXPoints - 1 );
    YProbeDist = workareaLenY / ( numYPoints - 1 );
    averageProbeDist = ( XProbeDist + YProbeDist ) / 2;

    if (requiredProbePoints() > maxProbePoints()) {
      options::maybe_throw(std::string("Required number of probe points (") + std::to_string(requiredProbePoints()) +
                           ") exceeds the maximum number (" + std::to_string(maxProbePoints()) + "). "
                           "Reduce either al-x or al-y.", ERR_INVALIDPARAMETER);
    }
}

void autoleveller::header(std::ofstream &of) {
    const char *logFileOpenAndComment[] = {
        "(PROBEOPEN RawProbeLog.txt) ( Record all probes in RawProbeLog.txt )",
        "M40 (Begins a probe log file, when the window appears, enter a name for the log file such as \"RawProbeLog.txt\")",
        "M40 (Begins a probe log file, when the window appears, enter a name for the log file such as \"RawProbeLog.txt\")"
    };
    const char *logFileClose[] = { "(PROBECLOSE)" , "M41", "M41" };

    if( software == Software::LINUXCNC )
        footerNoIf( of );

    if( tileInfo.enabled )
    {
        of << "#" << initialXOffsetVar << " = #5211\n";      //Save the initial offset
        of << "#" << initialYOffsetVar << " = #5212\n\n";
    }
    else
    {
        of << "#" << initialXOffsetVar << " = 0\n";
        of << "#" << initialYOffsetVar << " = 0\n\n";
    }
    of << probeOn << '\n';
    of << "G0 Z" << zsafe << " ( Move Z to safe height )\n";
    of << "G0 X" << startPointX << " Y" << startPointY << " ( Move XY to start point )\n";
    of << "G0 Z" << zprobe << " ( Move Z to probe height )\n";
    if( software != Software::CUSTOM )
        of << logFileOpenAndComment[software] << '\n';
    of << ( software == Software::CUSTOM ? probeCodeCustom : probeCode[software] ) << " Z" << zfail 
       << " F" << feedrate << " ( Z-probe )\n";
    of << "#500 = 0 ( Probe point [0, 0] is our reference )\n";
    of << ( software == Software::CUSTOM ? setZZeroCustom : setZZero[software] )
       << " ( Set the current Z as zero-value )\n";
    of << '\n';
    of << "( We now start the real probing: move the Z axis to the probing height, move to )\n";
    of << "( the probing XY position, probe it and save the result, parameter "
       << ( software == Software::CUSTOM ? zProbeResultVarCustom : zProbeResultVar[software] ) << ", )\n";
    of << "( in a numbered parameter; we will make " << numXPoints << " probes on the X-axis and )\n";
    of << "( " << numYPoints << " probes on the Y-axis, for a grand total of " << numXPoints * numYPoints << " probes )\n";
    of << '\n';

    if( software != Software::CUSTOM )
    {
        of << "#" << globalVar0 << " = 0 ( X iterator )\n";
        of << "#" << globalVar1 << " = 1 ( Y iterator )\n";
        of << "#" << globalVar2 << " = 1 ( UP or DOWN increment )\n";
        of << "#" << globalVar3 << " = " << numYPoints - 1 << " ( number of Y points; the 1st Y row can be done one time less )\n";
        of << silent_format( callSubRepeat[software] ) % xProbeNum % numXPoints % ocodes->getUniqueCode();
    }
    else
    {
      for (unsigned int i = 0; i < numXPoints; i++) {
        int j_start;
        int j_end;
        int j_direction;
        if (i % 2 == 0) {
          // Count upward.
          j_start = 0;
          j_end = numYPoints;
          j_direction = 1;
        } else {
          // Count downward.
          j_start = numYPoints - 1;
          j_end = -1;
          j_direction = -1;
        }
        if (i == 0) {
          j_start = 1; // Because the first probe was done above
        }
        for (int j = j_start; j != j_end; j += j_direction) {
          of << "G0 Z" << zprobe << '\n';
          of << "X" << i * XProbeDist + startPointX << " Y" << j * YProbeDist + startPointY << '\n';
          of << probeCodeCustom << " Z" << zfail << " F" << feedrate << '\n';
          of << getVarName(i, j) << "=" << zProbeResultVarCustom << '\n';
        }
      }
    }

    of << '\n';
    of << "G0 Z" << zsafe << " ( Move Z to safe height )\n";
    if( software != Software::CUSTOM )
       of << logFileClose[software] << " ( Close the probe log file )\n";
    of << "( Probing has ended, each Z-coordinate will be corrected with a bilinear interpolation )\n";
    of << probeOff << '\n';
    of << '\n';
}

void autoleveller::footerNoIf(std::ofstream &of) {
    const char *startSub[] = { "o%1$d sub", "O%1$d", "O%1$d" };
    const char *endSub[] = { "o%1$d endsub", "M99", "M99" };
    const char *var1[] = { "1", "1", globalVar0.c_str() };
    const char *var2[] = { "2", "2", globalVar1.c_str() };
    const char *var3[] = { "3", "3", globalVar2.c_str() };

    if(software != Software::CUSTOM) {
        of << format(startSub[software]) % g01InterpolatedNum << " ( G01 with Z-correction subroutine )\n";
        if( tileInfo.enabled )
        {
            of << "    #4 = [ #5211 - #" << initialXOffsetVar << " ] ( x-tile offset [minus the initial offset] )\n";
            of << "    #5 = [ #5212 - #" << initialYOffsetVar << " ] ( y-tile offset [minus the initial offset] )\n";
        }
        else
        {
            of << "    #4 = 0 ( x-tile offset [minus the initial offset] )\n";
            of << "    #5 = 0 ( y-tile offset [minus the initial offset] )\n";
        }
        of << "    #6 = [ FIX[ [ #" << var1[software] << " - " << startPointX << " + #4 ] / " << XProbeDist << " ] ] ( Lower left point X index )\n";
        of << "    #7 = [ FIX[ [ #" << var2[software] << " - " << startPointY << " + #5 ] / " << YProbeDist << " ] ] ( Lower left point Y index )\n";
        of << "    #8 = [ #6 * " << numYPoints << " + [ #7 + 1 ] + 500 ] ( Upper left point parameter number )\n";
        of << "    #9 = [ [ #6 + 1 ] *" << numYPoints << " + [ #7 + 1 ] + 500 ] ( Upper right point parameter number )\n";
        of << "    #10 = [ #6 * " << numYPoints << " + #7 + 500 ] ( Lower left point parameter number )\n";
        of << "    #11 = [ [ #6 + 1 ] * " << numYPoints << " + #7 + 500 ] ( Lower right point parameter number )\n";
        of << "    #12 = [ [ #" << var2[software] << " + #5 - " << startPointY << " - #7 * " << YProbeDist << " ] / " << YProbeDist << " ] "
           "( Distance between the point and the left border of the rectangle, normalized to 1 )\n";
        of << "    #13 = [ [ #" << var1[software] << " + #4 - " << startPointX << " - #6 * " << XProbeDist << " ] / " << XProbeDist << " ] "
           "( Distance between the point and the bottom border of the rectangle, normalized to 1 ) \n";
        of << "    #14 = [ ##10 + [ ##8 - ##10 ] * #12 ] ( Linear interpolation of the x-min elements )\n";
        of << "    #15 = [ ##11 + [ ##9 - ##11 ] * #12 ] ( Linear interpolation of the x-max elements )\n";
        of << "    #16 = [ #14 + [ #15 - #14 ] * #13 ] ( Linear interpolation of previously interpolated points )\n";
        of << "    G01 X#" << var1[software] << " Y#" << var2[software] << " Z[#" << var3[software] << " + #16]\n";
        of << silent_format(endSub[software]) % g01InterpolatedNum << endl;
        of << endl;
        of << format( startSub[software] ) % yProbeNum << " ( Y probe subroutine )\n";
        of << "    G0 Z" << zprobe << " ( Move to probe height )\n";
        of << "    X[#" << globalVar0 << " * " << XProbeDist << " + " << startPointX << "] Y[#" << globalVar1
           << " * " << YProbeDist << " + " << startPointY << "] ( Move to the current probe point )\n";
        of << "    " << probeCode[software] << " Z" << zfail << " F" << feedrate << " ( Probe it )\n";
        of << "    #[#" << globalVar0 << " * " << numYPoints << " + #" << globalVar1 << " + 500] = "
           << ( software == Software::CUSTOM ? zProbeResultVarCustom : zProbeResultVar[software] )
           << " ( Save the probe in the correct parameter )\n";
        of << "    #" << globalVar1 << " = [#" << globalVar1 << " + #" << globalVar2 << "] ( Increment/decrement by 1 the Y counter )\n";
        of << silent_format( endSub[software] ) % yProbeNum << endl;
        of << endl;
        of << format( startSub[software] ) % xProbeNum << " ( X probe subroutine )\n";
        of << "    " << silent_format( callSubRepeat[software] ) % yProbeNum % ( "#" + globalVar3 ) % ocodes->getUniqueCode() % "    ";
        of << "    #" << globalVar3 << " = " << numYPoints << endl;
        of << "    #" << globalVar2 << " = [0 - #" << globalVar2 << "]\n";
        of << "    #" << globalVar1 << " = [#" << globalVar1 << " + #" << globalVar2 << ']' << endl;
        of << "    #" << globalVar0 << " = [#" << globalVar0 << " + 1] ( Increment by 1 the X counter )\n";
        of << silent_format( endSub[software] ) % xProbeNum << endl;
        of << endl;
    }
}

template <typename T>
static inline T clamp(const T& x, const T& min_x, const T& max_x) {
  return std::max(min_x, std::min(x, max_x));
}

string autoleveller::interpolatePoint ( point_type_fp point )
{
    unsigned int xminindex;
    unsigned int yminindex;
    double x_minus_x0_rel;
    double y_minus_y0_rel;

    // Calculate point index for measurement point below and to the left
    // of `point`
    xminindex = floor((point.x() - startPointX) / XProbeDist);
    xminindex = clamp(xminindex, 0U, numXPoints - 1);

    yminindex = floor((point.y() - startPointY) / YProbeDist);
    yminindex = clamp(yminindex, 0U, numYPoints - 1);

    // Get fraction of the distance to the next measurement point up and
    // to the left that `point` is.
    x_minus_x0_rel = ( point.x() - startPointX - xminindex * XProbeDist ) / XProbeDist;
    y_minus_y0_rel = ( point.y() - startPointY - yminindex * YProbeDist ) / YProbeDist;

    if ( y_minus_y0_rel == 0 ) {

        if ( x_minus_x0_rel == 0 ) {

            // If `point` is on top of a measurement point, just copy
            // the measured height over
            return str( format( "#%2$s=%1$s\n" ) %
                        getVarName( xminindex, yminindex ) %
						returnVar );

        } else {

            // If `point` has the same y coordinate as a row of points,
            // interpolate between the points to the left and right of
            // it
            return str( format( "#%4$s=[%1$s+[%2$s-%1$s]*%3$.5f]\n" ) %
                        getVarName( xminindex, yminindex ) %
                        getVarName( xminindex + 1, yminindex ) %
                        x_minus_x0_rel % returnVar );

        }

    } else {

        if ( x_minus_x0_rel == 0 ) {

            // If `point` has the same x coordinate as a column of
            // points, interpolate between the points above and below it
            return str( format( "#%4$s=[%2$s+[%1$s-%2$s]*%3$.5f]\n" ) %
                        getVarName( xminindex, yminindex + 1 ) %
                        getVarName( xminindex, yminindex ) %
                        y_minus_y0_rel % returnVar );

        } else {

            // ...else use bilinear interpolation between all four
            // points around it
            return str( format( "#%7$s=[%3$s+[%1$s-%3$s]*%5$.5f]\n#%8$s=[%4$s+[%2$s-%4$s]*%5$.5f]\n#%9$s=[#%7$s+[#%8$s-#%7$s]*%6$.5f]\n" ) %
                        getVarName( xminindex, yminindex + 1 ) %
                        getVarName( xminindex + 1, yminindex + 1 ) %
                        getVarName( xminindex, yminindex ) %
                        getVarName( xminindex + 1, yminindex ) %
                        y_minus_y0_rel % x_minus_x0_rel % 
                        globalVar4 % globalVar5 % returnVar );

        }

    }

}


static inline point_type_fp operator*(const point_type_fp& a, const point_type_fp& b) {
  return point_type_fp(a.x() * b.x(), a.y() * b.y());
}

static inline point_type_fp operator/(const point_type_fp& a, const point_type_fp& b) {
  return point_type_fp(a.x() / b.x(), a.y() / b.y());
}


linestring_type_fp partition_segment(const point_type_fp& source, const point_type_fp& dest,
                                     const point_type_fp& grid_zero, const point_type_fp& grid_width) {
  if (source == dest) {
    return {dest};
  }
  double current_progress = 0;
  linestring_type_fp points;
  while (current_progress != 1) {
    const auto current = source + (dest - source) * current_progress;
    points.push_back(current);

    const auto current_index = floor((current - grid_zero) / grid_width);
    double best_progress = 1;
    for (const auto& index_delta : {-1,0,1,2}) {
      const auto new_point = (current_index + point_type_fp(index_delta, index_delta)) * grid_width + grid_zero;
      const auto new_progress = (new_point - source) / (dest - source);
      if (new_progress.x() > current_progress && new_progress.x() < best_progress) {
        // This step gets us closer to the end yet moves the least amount in that direction.
        best_progress = new_progress.x();
      }
      if (new_progress.y() > current_progress && new_progress.y() < best_progress) {
        // This step gets us closer to the end yet moves the least amount in that direction.
        best_progress = new_progress.y();
      }
    }
    current_progress = best_progress;
  }
  points.push_back(dest);
  return points;
}

string autoleveller::addChainPoint (point_type_fp point, double zwork) {
    string outputStr;
    linestring_type_fp subsegments;
    linestring_type_fp::const_iterator i;

    subsegments = partition_segment(lastPoint, point, point_type_fp(startPointX, startPointY), point_type_fp(XProbeDist, YProbeDist));

    if (software == Software::LINUXCNC || software == Software::MACH4 || software == Software::MACH3) {
      for( i = subsegments.begin() + 1; i != subsegments.end(); i++ )
        outputStr += str( silent_format( callSub2[software] ) % g01InterpolatedNum % i->x() % i->y() % zwork);
    } else {
      for(i = subsegments.begin() + 1; i != subsegments.end(); i++) {
        outputStr += interpolatePoint( *i );
        outputStr += str( format( "X%1$.5f Y%2$.5f Z[#%4$s+%3$.5f]\n" ) % i->x() % i->y() % zwork % returnVar );
      }
    }

    lastPoint = point;
    return outputStr;
}

string autoleveller::g01Corrected (point_type_fp point, double zwork) {
  if( software == Software::LINUXCNC || software == Software::MACH4 || software == Software::MACH3 ) {
    return str( silent_format( callSub2[software] ) % g01InterpolatedNum % point.x() % point.y() % zwork);
  } else {
    return interpolatePoint( point ) + "G01 Z[" + str(format("%.5f")%zwork) + "+#" + returnVar + "]\n";
  }
}
