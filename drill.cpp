/*
 * This file is part of pcb2gcode.
 *
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 * Copyright (C) 2010 Bernhard Kubicek <kubicek@gmx.at>
 * Copyright (C) 2013 Erik Schuster <erik@muenchen-ist-toll.de>
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

#include <fstream>
using std::ofstream;

#include <cstring>

#include <iostream>
using std::cout;
using std::endl;
using std::flush;

#include <vector>
using std::vector;

#include <sstream>
using std::stringstream;

#include <memory>
using std::shared_ptr;

#include <numeric>
#include <iomanip>
using std::setprecision;
using std::fixed;

#include <boost/format.hpp>
using boost::format;

#include <string>
using std::string;

#include <map>
using std::map;

#include "drill.hpp"
#include "tsp_solver.hpp"
#include "common.hpp"
#include "units.hpp"
#include "available_drills.hpp"
#include "bg_operators.hpp"

using std::pair;
using std::make_pair;
using std::min_element;
using std::cerr;
using std::ios_base;
using std::left;
using std::to_string;

/******************************************************************************/
/*
 Constructor
 metricoutput : if true, ngc output in metric units
 */
/******************************************************************************/
ExcellonProcessor::ExcellonProcessor(const boost::program_options::variables_map& options,
                                     const point_type_fp min,
                                     const point_type_fp max)
  : board_dimensions(min, max),
    board_center_x(((min + max)/2).x()),
    project(parse_project(options["drill"].as<string>())),
    bMetricOutput(options["metricoutput"].as<bool>()),
    parsed_bits(parse_bits()),
    parsed_holes(parse_holes()),
    drillfront(workSide(options, "drill")),
    inputFactor(options["metric"].as<bool>() ? 1.0/25.4 : 1),
    tsp_2opt(options["tsp-2opt"].as<bool>()),
    xoffset((options["zero-start"].as<bool>() ? min.x() : 0) -
            options["x-offset"].as<Length>().asInch(inputFactor)),
    yoffset((options["zero-start"].as<bool>() ? min.y() : 0) -
            options["y-offset"].as<Length>().asInch(inputFactor)),
    mirror_axis(options["mirror-axis"].as<Length>()),
    mirror_yaxis(options["mirror-yaxis"].as<bool>()),
    min_milldrill_diameter(options["min-milldrill-hole-diameter"].as<Length>()),
    min_milldrill_entry_diameter(options["min-milldrill-entry-diameter"].as<Percent>()),
    max_milldrill_entry_diameter(options["max-milldrill-entry-diameter"].as<Percent>()),
    milldrill_stepover(options["milldrill-stepover"].as<Percent>()),
    mill_feed_direction(options["mill-feed-direction"].as<MillFeedDirection::MillFeedDirection>()),
    available_drills(flatten(options["drills-available"].as<std::vector<AvailableDrills>>())),
    ocodes(1),
    globalVars(100),
    tileInfo(Tiling::generateTileInfo(options, max.y() - min.y(), max.x() - min.x())) {
    //set imperial/metric conversion factor for output coordinates depending on metricoutput option
    cfactor = bMetricOutput ? 25.4 : 1;

    //set metric or imperial preambles
    if (bMetricOutput)
    {
        preamble = string("G94       (Millimeters per minute feed rate.)\n")
                   + "G21       (Units == Millimeters.)\n";
    }
    else
    {
        preamble = string("G94       (Inches per minute feed rate.)\n")
                   + "G20       (Units == INCHES.)\n";
    }

    if (!options["nog91-1"].as<bool>())
        preamble += "G91.1     (Incremental arc distance mode.)\n";

    preamble += "G90       (Absolute coordinates.)\n";

    tiling = std::make_unique<Tiling>(tileInfo, cfactor, ocodes.getUniqueCode());
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::add_header(string header)
{
    this->header.push_back(header);
}

/******************************************************************************/
/*
 Recalculates the x-coordinate based on drillfront
 drillfront: drill from front side
 xvalue: x-coordinate
 returns the recalulated x-coordinate
 */
/******************************************************************************/
double ExcellonProcessor::get_xvalue(double xvalue)
{
    double retval;

    if (drillfront || mirror_yaxis) //drill from the front, no calculation needed
    {
        retval = xvalue;
    }
    else
    {
        retval = 2 * (xoffset + mirror_axis.asInch(inputFactor)) - xvalue - (tileInfo.tileX-1) * (tileInfo.boardWidth);
    }

    return retval;
}

double ExcellonProcessor::get_yvalue(double yvalue)
{
    double retval;

    if (drillfront || !mirror_yaxis)
    {
        retval = yvalue;
    }
    else
    {
        retval = 2 * (yoffset + mirror_axis.asInch(inputFactor)) - yvalue - (tileInfo.tileY-1) * (tileInfo.boardHeight);
    }

    return retval;
}

string ExcellonProcessor::drill_to_string(drillbit drillbit) {
    auto diameter = drillbit.diameter;
    if (drillbit.unit == "mm" && !bMetricOutput) {
        diameter /= 25.4; // convert mm to inches
    } else if (drillbit.unit != "mm" && bMetricOutput) {
        diameter *= 25.4; // convert inches to mm
    }
    auto unit = bMetricOutput ? "mm" : "inch";
    std::ostringstream ss;
    ss << diameter << unit;
    return ss.str();
}

linestring_type_fp ExcellonProcessor::line_to_holes(const linestring_type_fp& line, double drill_diameter) {
    auto start_x = line.front().x();
    auto start_y = line.front().y();
    auto stop_x = line.back().x();
    auto stop_y = line.back().y();
    auto distance = sqrt((stop_x-start_x)*(stop_x-start_x)+
                         (stop_y-start_y)*(stop_y-start_y));
    // According to the spec for G85, holes should be drilled so that
    // protrusions are no larger than 0.0005inches.  The formula below
    // determines the maximum distance between drill centers.
    const double max_protrusion = 0.0005;
    double step_size = sqrt(4*max_protrusion*(drill_diameter-max_protrusion));
    // The number of holes that need to be drilled. 0 is at start,
    // drill_count-1 at the stop.  Evenly spaced.
    const unsigned int drill_count = ((unsigned int) ceil(distance/step_size)) + 1;
    // drills_to_do has pairs where is pair is the inclusive range of
    // drill holes that still need to be made.  We try to drill in a
    // way so that the pressure on the drill is balanced.
    vector<pair<int, int>> drills_to_do;
    // drill the start point
    drills_to_do.push_back(std::make_pair(0, 0));
    if (drill_count > 1) {
        // drill the stop point
        drills_to_do.push_back(std::make_pair(drill_count - 1, drill_count - 1));
    }
    // drill all the rest
    drills_to_do.push_back(std::make_pair(1, drill_count-2));
    linestring_type_fp holes;
    for (unsigned int current_drill_index = 0;
         current_drill_index < drills_to_do.size();
         current_drill_index++) {
        const auto& current_drill = drills_to_do[current_drill_index];
        const int start_drill = current_drill.first;
        const int end_drill = current_drill.second;
        if (start_drill > end_drill) {
            continue;
        }
        // find a point between start and end inclusive.
        const int mid_drill = (start_drill+1)/2 + end_drill/2;
        // drill the point that is the percentage between start and stop
        double ratio = drill_count > 1 ? mid_drill / (drill_count-1.) : 0;
        const auto x = start_x * (1 - ratio) + stop_x * ratio;
        const auto y = start_y * (1 - ratio) + stop_y * ratio;
        drills_to_do.push_back(std::make_pair(start_drill, mid_drill-1));
        drills_to_do.push_back(std::make_pair(mid_drill+1, end_drill));
        holes.push_back(point_type_fp(x, y));
    }
    return holes;
}

/******************************************************************************/
/*
 Exports the ngc file for drilling
 of_name: output filename
 driller: ...
 onedrill: if true, only the first drill bit is used, the others are skipped

 TODO: Replace the current tiling implementation (gcode repetition) with a subroutine-based solution
 */
/******************************************************************************/
void ExcellonProcessor::export_ngc(const string of_dir, const boost::optional<string>& of_name,
                                   shared_ptr<Driller> driller, bool onedrill,
                                   bool nog81, bool nom6, bool zchange_absolute) {
    stringstream zchange;

    cout << "Exporting drill... ";

    zchange << setprecision(3) << fixed << driller->zchange * cfactor;

    tiling->setGCodeEnd((zchange_absolute ? "G53 " : "") + string("G00 Z") + zchange.str() +
                         " ( All done -- retract )\n" + postamble_ext +
                         "\nM5      (Spindle off.)\nG04 P" +
                         to_string(driller->spindown_time) +
                        "\nM9      (Coolant off.)\n"
                         "M2      (Program end.)\n\n");

    map<int, drillbit> bits = optimize_bits();
    const auto holes = optimize_holes(bits, onedrill, boost::none, min_milldrill_diameter);

    //open output file
    std::ofstream of;
    if (of_name && holes.size() > 0) {
      auto filename = build_filename(of_dir, *of_name);
      of.open(filename);
      if (!of.is_open()) {
        std::stringstream error_message;
        error_message << "Can't open for writing: " << filename;
        throw std::invalid_argument(error_message.str());
      }
    }
    //write header to .ngc file
    for (string s : header)
    {
        of << "( " << s << " )" << "\n";
    }

    of << "( Software-independent Gcode )\n";

    if (!onedrill)
    {
        of << "\n( This file uses " << holes.size() << " drill bit sizes. )\n";
        of << "( Bit sizes:";
        for (const auto& hole : holes) {
            const auto& bit = bits.at(hole.first);
            of << " [" << drill_to_string(bit) << "]";
        }
        of << " )\n\n";
    }
    else
    {
        of << "\n( This file uses only one drill bit. Forced by 'onedrill' option )\n\n";
    }

    of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
    of.precision(5);           //Set floating-point decimal precision

    of << preamble_ext;        //insert external preamble file
    of << preamble;            //insert internal preamble
    of << "G00 S" << left << driller->speed << "     (RPM spindle speed.)\n" << "\n";

    //tiling->header( of );     // See TODO #2

    for (const auto& hole : holes) {
        const auto& bit = bits.at(hole.first);
        if (zchange_absolute) {
            of << "G53 ";
        }
        of << "G00 Z" << driller->zchange * cfactor << " (Retract)\n" << "T"
           << hole.first << "\n" << "M5      (Spindle stop.)\n"
           << "G04 P" << driller->spindown_time
           << "\n(MSG, Change tool bit to drill size "
           << drill_to_string(bit) << ")\n"
           << (nom6?"":"M6      (Tool change.)\n")
           << "M0      (Temporary machine stop.)\n"
           << "M3      (Spindle on clockwise.)\n"
           << "G0 Z" << driller->zsafe * cfactor << "\n"
           << "G04 P" << driller->spinup_time << "\n\n";

        if( nog81 )
            of << "G1 F" << driller->feed * cfactor << '\n';
        else
        {
            of << "G81 R" << driller->zsafe * cfactor << " Z"
               << driller->zwork * cfactor << " F" << driller->feed * cfactor << " ";
        }

        double drill_diameter = bit.unit == "mm" ? bit.diameter / 25.4 : bit.diameter;
        for( unsigned int i = 0; i < tileInfo.tileY; i++ )
        {
            const double yoffsetTot = yoffset - i * tileInfo.boardHeight;

            for( unsigned int j = 0; j < tileInfo.tileX; j++ )
            {
                const double xoffsetTot = xoffset - ( i % 2 ? tileInfo.tileX - j - 1 : j ) * tileInfo.boardWidth;

                for (const auto& line : hole.second) {
                    for (auto& drill_hole : line_to_holes(line, drill_diameter)) {
                        const auto x = drill_hole.x();
                        const auto y = drill_hole.y();

                        if( nog81 )
                        {
                            of << "G0 X" << ( ( get_xvalue(x) - xoffsetTot ) * cfactor)
                               <<   " Y" << ( ( get_yvalue(y) - yoffsetTot ) * cfactor) << "\n";
                            of << "G1 Z" << driller->zwork * cfactor << '\n';
                            of << "G1 Z" << driller->zsafe * cfactor << '\n';
                        }
                        else
                        {
                            of << "X" << ( ( get_xvalue(x) - xoffsetTot ) * cfactor)
                              << " Y" << ( ( get_yvalue(y) - yoffsetTot ) * cfactor) << "\n";
                        }
                    }
                }
            }
        }
        if (!nog81) {
          of << "G80\n"; // End the G81 from before.
        }
        of << "\n";
    }

    //tiling->footer( of ); // See TODO #2
    of << tiling->getGCodeEnd();

    of.close();

    save_svg(bits, holes, of_dir, "original_drill.svg");
}

/******************************************************************************/
/*
 *  mill one circle
 *  if last_pass, return to zsafe, otherwise return to start height for another pass
 */
/******************************************************************************/
void ExcellonProcessor::millhole_one(std::ofstream &of,
                                     double start_x, double start_y,
                                     double stop_x, double stop_y,
                                     shared_ptr<Cutter> cutter,
                                     double holediameter, bool last_pass)
{

    g_assert(cutter);
    double cutdiameter = cutter->tool_diameter;
    bool slot = (start_x != stop_x ||
                 start_y != stop_y);

    // Find the largest z_step that divides 0 through z_work into
    // evenly sized passes such that each pass is at most
    // cutter->stepsize in depth.
    int stepcount = (int) ceil(std::abs(cutter->zwork / cutter->stepsize));

    double start_z = -1.0/stepcount * cutter->zwork;
    double return_z = last_pass ? cutter->zsafe : start_z;

    double delta_x = stop_x - start_x;
    double delta_y = stop_y - start_y;
    double distance = sqrt(delta_x*delta_x + delta_y*delta_y);
    if (cutdiameter * 1.001 >= holediameter) { //In order to avoid a "zero radius arc" error
        // Hole is smaller than cutdiameter so just drill/zig-zag.
        of << "G0 X" << start_x * cfactor << " Y" << start_y * cfactor << '\n';
        if (slot) {
            // Start one step above Z0 for optimal entry
            of << "G1 Z" << start_z * cfactor
               << " F" << cutter->vertfeed * cfactor << '\n';

            // Is there enough room for material evacuation?
            if (distance > 0.3 * cutdiameter) {
                of << "G1 F" << cutter->feed * cfactor << '\n';
            }

            double zhalfstep = cutter->zwork / stepcount / 2;
            for (int current_step = -1; true; current_step++) {
                // current_step == stepcount is for the bottom pass, so z needs to stay the same
                double z = double(std::min(stepcount, current_step+1))/stepcount * cutter->zwork;
                of << "G1 X" << stop_x * cfactor
                   << " Y" << stop_y * cfactor;
                if(stepcount != current_step) {
                   // Drop superfluous Z from the bottom pass to indicate that this line is not there by accident
                   of << " Z" << (z - zhalfstep) * cfactor;
                }
                of << '\n';
                // We don't need a second "zag" on the bottom pass
                if (current_step >= stepcount) {
                    break;
                }
                of << "G1 X" << start_x * cfactor
                   << " Y" << start_y * cfactor
                   << " Z" << z * cfactor
                   << '\n';
            }
        } else {
            of << "G1 Z" << cutter->zwork * cfactor
               << " F" << cutter->vertfeed * cfactor << '\n';
        }
        of << "G1 Z" << return_z * cfactor
           << " F" << cutter->vertfeed * cfactor << "\n\n";
    } else {
        // Hole is larger than cutter diameter so make circles/ovals.
        double millr = (holediameter - cutdiameter) / 2.;      //mill radius
        double mill_x;
        double mill_y;
        if (slot) {
          mill_x = delta_x*millr/distance;
          mill_y = delta_y*millr/distance;
        } else {
          // No distance so just use a start that is directly north
          // of the start.
          mill_x = 0;
          mill_y = millr;
        }
        // We will draw a shape that looks like a rectangle with
        // half circles attached on just two opposite sides.
        if (mill_feed_direction == MillFeedDirection::CLIMB) {
          // Negate the variables for rotating in the opposite direction.
          mill_y = -mill_y;
          mill_x = -mill_x;
        }
        // add delta rotated 90 degrees CW/CCW then normalize to length millr
        double start_targetx = start_x + mill_y;
        double start_targety = start_y - mill_x;
        // add delta rotated 90 degrees CCW/CW then normalize to length millr
        double start2_targetx = start_x - mill_y;
        double start2_targety = start_y + mill_x;
        // add delta rotated 90 degrees CCW/CW then normalize to length millr
        double stop_targetx = stop_x - mill_y;
        double stop_targety = stop_y + mill_x ;
        // add delta rotated 90 degrees CW/CCW then normalize to length millr
        double stop2_targetx = stop_x + mill_y;
        double stop2_targety = stop_y - mill_x;

        of << "G0 X" << start_targetx * cfactor << " Y" << start_targety * cfactor << '\n';

        // Distribute z step depth on half circles and straight lines for slots
        double zdiff_hcircle1 = 0;
        double zdiff_line1 = 0;
        double zdiff_hcircle2 = 0;
        // Distance traveled by one half circle
        double dist_hcircle = boost::math::constants::pi<double>() * millr;
        if (slot) {
          // How much to step down per pass
          double zstep = cutter->zwork / stepcount;
          double zstep_hcircle = zstep * dist_hcircle / (dist_hcircle + distance) / 2;
          double zstep_line    = zstep / 2 - zstep_hcircle;
          // How much to substract from the final z depth of each pass
          zdiff_hcircle1 = zstep - zstep_hcircle;
          zdiff_line1    = zstep / 2;
          zdiff_hcircle2 = zstep_line;
        }

        // Start one step above Z0 for optimal entry
        of << "G1 Z" << start_z * cfactor
           << " F" << cutter->vertfeed * cfactor << '\n';

        // Is hole is big enough for horizontal speed?
        if (holediameter + distance > 1.1 * cutdiameter) {
          of << "G1 F" << cutter->feed * cfactor << '\n';
        }

        string arc_gcode = mill_feed_direction == MillFeedDirection::CLIMB ? "G3" : "G2";
        for (int current_step = -1; current_step <= stepcount; current_step++) {
          // current_step == stepcount is for the bottom circle for helix, so z needs to stay the same
          double z = double(std::min(stepcount, current_step+1))/stepcount * cutter->zwork;
          if (!slot) {
            // Just drill a full-circle.
            of << arc_gcode
               << " X" << start_targetx * cfactor
               << " Y" << start_targety * cfactor;
            if(stepcount != current_step) {
               // Drop superfluous Z from the bottom hole to indicate that this line is not there by accident
               of << " Z" << z * cfactor;
            }
            of << " I" << (start_x-start_targetx) * cfactor
               << " J" << (start_y-start_targety) * cfactor << "\n";
          } else {
            // Draw the first half circle
            of << arc_gcode << " X" << start2_targetx * cfactor
               << " Y" << start2_targety * cfactor;
            if(stepcount != current_step) {
              of << " Z" << (z - zdiff_hcircle1) * cfactor;
            }
            of << " I" << (start_x-start_targetx) * cfactor
               << " J" << (start_y-start_targety) * cfactor << "\n";
            // Now across to the second half circle
            of << "G1 X" << stop_targetx * cfactor
               << " Y" << stop_targety * cfactor;
            if(stepcount != current_step) {
              of << " Z" << (z - zdiff_line1) * cfactor;
            }
            of << "\n";
            // Draw the second half circle
            of << arc_gcode << " X" << stop2_targetx * cfactor
               << " Y" << stop2_targety * cfactor;
            if(stepcount != current_step) {
              of << " Z" << (z - zdiff_hcircle2) * cfactor;
            }
            of << " I" << (stop_x-stop_targetx) * cfactor
               << " J" << (stop_y-stop_targety) * cfactor << "\n";
            // Now back to the start of the first half circle
            of << "G1 X" << start_targetx * cfactor
               << " Y" << start_targety * cfactor;
            if(stepcount != current_step) {
              of << " Z" << z * cfactor;
            }
            of << "\n";
          }
        }

        of << "G1 Z" << return_z * cfactor
           << " F" << cutter->vertfeed * cfactor << "\n\n";
    }
}

// mill a hole im multiple passes; returns false if tool is bigger than the circle
bool ExcellonProcessor::millhole(std::ofstream &of, double start_x, double start_y,
                                 double stop_x, double stop_y,
                                 shared_ptr<Cutter> cutter,
                                 double holediameter) {
    g_assert(cutter);

    // mill a holediameter size hole in multiple steps, starting from a size
    // between min_entry and max_entry, enlarging by less than diameter_step each
    // time
    double min_entry = cutter->tool_diameter * min_milldrill_entry_diameter.asFraction(1);
    double max_entry = cutter->tool_diameter * max_milldrill_entry_diameter.asFraction(1);
    double diameter_step = cutter->tool_diameter * milldrill_stepover.asFraction(1) * 2.0;

    // number of diameter_step size enlargements to go from a size under
    // max_entry to holediameter without exceeding diameter_step. tolerance
    // ensures that at the boundaries the number of steps is as expected
    int steps = (int)std::ceil((holediameter - max_entry) / diameter_step - 0.001);

    if (steps <= 0) {
        // can do this in one pass
        millhole_one(of, start_x, start_y, stop_x, stop_y, cutter, holediameter);
    } else {
        // adjust entry diameter and reduce step size to meet requirements
        double entry_diameter = holediameter - steps * diameter_step;
        entry_diameter = std::max(entry_diameter, min_entry);
        // generally already met, except it can be slightly over in tolerance region
        entry_diameter = std::min(entry_diameter, max_entry);

        diameter_step = (holediameter - entry_diameter) / steps;

        for (int step = 0; step <= steps; step++) {
            millhole_one(of, start_x, start_y, stop_x, stop_y, cutter,
                         entry_diameter + step * diameter_step, step == steps);
        }
    }

    // tolerance to ensure we return true if the cutter and hole are the same size
    return holediameter > cutter->tool_diameter * 0.999;
}

// milldrill holes
void ExcellonProcessor::export_ngc(const string of_dir, const boost::optional<string>& of_name,
                                   shared_ptr<Cutter> target, bool zchange_absolute) {
    unsigned int badHoles = 0;
    stringstream zchange;

    cout << "Exporting milldrill... " << flush;

    zchange << setprecision(6) << fixed << target->zchange * cfactor;
    tiling->setGCodeEnd((zchange_absolute ? "G53 " : "") + string("G00 Z") + zchange.str() +
                        " ( All done -- retract )\n" + postamble_ext +
                        "\nM5      (Spindle off.)\nG04 P" +
                        to_string(target->spindown_time) +
                        "\nM9      (Coolant off.)\n"
                        "M2      (Program end.)\n\n");

    map<int, drillbit> bits = parsed_bits;
    const auto holes =
        optimize_holes(bits, false, min_milldrill_diameter, boost::none);

    // open output file
    std::ofstream of;
    if (of_name && holes.size() > 0) {
      auto filename = build_filename(of_dir, *of_name);
      of.open(filename);
      if (!of.is_open()) {
        std::stringstream error_message;
        error_message << "Can't open for writing: " << filename;
        throw std::invalid_argument(error_message.str());
      }
    }

    // write header to .ngc file
    for (string s : header)
    {
        of << "( " << s << " )" << "\n";
    }

    if( tileInfo.enabled && tileInfo.software != Software::CUSTOM )
        of << "( Gcode for " << tileInfo.software << " )\n";
    else
        of << "( Software-independent Gcode )\n";

    of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
    of.precision(5);              //Set floating-point decimal precision

    of << "( This file uses a mill head of " << (bMetricOutput ? (target->tool_diameter * 25.4) : target->tool_diameter)
       << (bMetricOutput ? "mm" : "inch") << " to drill the " << holes.size()
       << " hole sizes. )" << "\n";

    of << "( Hole sizes:";
    for (const auto& hole : holes) {
        const auto& bit = bits.at(hole.first);
        of << " [" << drill_to_string(bit) << "]";
    }
    of << " )\n\n";

    //preamble
    of << preamble_ext << preamble
       << "S" << left << target->speed << "    (RPM spindle speed.)\n\n"
       << "G01 F" << target->feed * cfactor << " (Feedrate)\n";
    if (zchange_absolute) {
       of << "G53 ";
    }
    of << "G00 Z" << target->zchange * cfactor << " (Retract to tool change height)\n"
       << "T" << (holes.size() > 0 ? (*holes.begin()).first : 0) << "\n"
       << "M5        (Spindle stop.)\n"
       << "G04 P" << target->spindown_time << "\n"
       << "(MSG, Change tool bit to drill size " << (bMetricOutput ? (target->tool_diameter * 25.4) : target->tool_diameter) << (bMetricOutput ? "mm" : "inch") << ")\n"
       << "M6        (Tool change.)\n"
       << "M0        (Temporary machine stop.)\n"
       << "M3        (Spindle on clockwise.)\n"
       << "G04 P" << target->spinup_time << "\n"
       << "G00 Z" << target->zsafe * cfactor << "\n\n";

    tiling->header( of );

    for( unsigned int i = 0; i < tileInfo.forYNum; i++ )
    {
        const double yoffsetTot = yoffset - i * tileInfo.boardHeight;

        for( unsigned int j = 0; j < tileInfo.forXNum; j++ )
        {
            const double xoffsetTot = xoffset - ( i % 2 ? tileInfo.forXNum - j - 1 : j ) * tileInfo.boardWidth;

            if( tileInfo.enabled && tileInfo.software == Software::CUSTOM )
                of << "( Piece #" << j + 1 + i * tileInfo.forXNum << ", position [" << j << ";" << i << "] )\n\n";

            for (const auto& hole : holes) {
                const auto& bit = bits.at(hole.first);
                double diameter = bit.unit == "mm" ? bit.diameter / 25.4 : bit.diameter;
                for (const auto& line : hole.second) {
                    const auto& start_x = line.front().x();
                    const auto& start_y = line.front().y();
                    const auto& end_x = line.back().x();
                    const auto& end_y = line.back().y();
                    if (!millhole(of,
                                  get_xvalue(start_x) - xoffsetTot, get_yvalue(start_y) - yoffsetTot,
                                  get_xvalue(end_x  ) - xoffsetTot, get_yvalue(  end_y) - yoffsetTot,
                                  target, diameter)) {
                        ++badHoles;
                    }
                }
            }
        }
    }

    tiling->footer( of );

    of.close();

    if( badHoles != 0 )
    {
        badHoles /= tileInfo.tileX * tileInfo.tileY;    //Don't count the same bad hole multiple times
        cerr << "Warning: " << badHoles << ( badHoles == 1 ? " hole was" : " holes were" )
             << " bigger than the milling tool." << endl;
    }

    save_svg(bits, holes, of_dir, "original_milldrill.svg");
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::save_svg(
    const map<int, drillbit>& bits,
    const vector<pair<int, multi_linestring_type_fp>>& holes,
    const string& of_dir, const string& of_name) {
    if (holes.size() == 0) {
      return;
    }
    const coordinate_type_fp width = (board_dimensions.max_corner().x() - board_dimensions.min_corner().x()) * SVG_PIX_PER_IN;
    const coordinate_type_fp height = (board_dimensions.max_corner().y() - board_dimensions.min_corner().y()) * SVG_PIX_PER_IN;
    const coordinate_type_fp viewBox_width = (board_dimensions.max_corner().x() - board_dimensions.min_corner().x()) * SVG_DOTS_PER_IN;
    const coordinate_type_fp viewBox_height = (board_dimensions.max_corner().y() - board_dimensions.min_corner().y()) * SVG_DOTS_PER_IN;

    //Some SVG readers does not behave well when viewBox is not specified
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"0 0 %3% %4%\"") % width % height % viewBox_width % viewBox_height);

    ofstream svg_out (build_filename(of_dir, of_name));
    bg::svg_mapper<point_type_fp> mapper (svg_out, viewBox_width, viewBox_height, svg_dimensions);

    mapper.add(board_dimensions);

    for (const auto& hole : holes) {
        const auto& bit = bits.at(hole.first);
        const double radius = bit.unit == "mm" ? (bit.diameter / 25.4) / 2 : bit.diameter / 2;

        for (const linestring_type_fp& line : hole.second) {
            for (auto& hole : line_to_holes(line, radius*2)) {
                mapper.map(hole, "", radius * SVG_DOTS_PER_IN);
            }
        }
    }
}

std::unique_ptr<gerbv_project_t, ExcellonProcessor::GerbvDeleter> ExcellonProcessor::parse_project(const string& filename) {
  auto project = std::unique_ptr<gerbv_project_t, GerbvDeleter>(gerbv_create_project());
  auto gerb_filename = std::make_unique<char[]>(filename.size() + 1);
  strcpy(gerb_filename.get(), filename.c_str());

  gerbv_open_layer_from_filename(project.get(), gerb_filename.get());

  if (project->file[0] == NULL) {
    throw drill_exception();
  }
  return project;
}

map<int, drillbit> ExcellonProcessor::parse_bits() {
  map<int, drillbit> bits;

  for (gerbv_drill_list_t* currentDrill =
           project->file[0]->image->drill_stats->drill_list;
       currentDrill;
       currentDrill = currentDrill->next) {
    drillbit curBit;
    curBit.diameter = currentDrill->drill_size;
    curBit.unit = currentDrill->drill_unit == nullptr ? "" : string(currentDrill->drill_unit);
    curBit.drill_count = currentDrill->drill_count;

    bits.insert(pair<int, drillbit>(currentDrill->drill_num, curBit));
  }
  return bits;
}

// Must be called after parse bits so that we can report on unused bits.
map<int, multi_linestring_type_fp> ExcellonProcessor::parse_holes() {
  map<int, multi_linestring_type_fp> holes;

  for (gerbv_net_t* currentNet = project->file[0]->image->netlist; currentNet;
       currentNet = currentNet->next) {
    if (currentNet->aperture != 0)
      holes[currentNet->aperture].push_back(
          linestring_type_fp{point_type_fp(currentNet->start_x, currentNet->start_y),
                             point_type_fp(currentNet->stop_x, currentNet->stop_y)});
  }
  // Report all bits that are unused as warnings.
  for (const auto& bit : parsed_bits) {
    if (holes.count(bit.first) == 0) { //If a bit has no associated holes
      cerr << "Warning: bit " << bit.first << " ("
           << drill_to_string(bit.second) << ") has no associated holes; "
          "removing it." << std::endl;
      // We don't really remove the bit.  If there are no holes
      // associated, we'll remove the hole later and there will be no
      // output for the bit.
    }
  }
  return holes;
}

/******************************************************************************/
/*
 Optimisation of the hole path with a TSP Nearest Neighbour algorithm
 */
/******************************************************************************/
vector<pair<int, multi_linestring_type_fp>> ExcellonProcessor::optimize_holes(
    map<int, drillbit>& bits, bool onedrill,
    const boost::optional<Length>& min_diameter,
    const boost::optional<Length>& max_diameter) {
  map<int, multi_linestring_type_fp> holes(parsed_holes);

  // Holes that are larger than max_diameter or smaller than min_diameter are removed.
  for (auto path = holes.begin(); path != holes.end(); ) {
    const auto& bit_diameter = bits.at(path->first).as_length().asInch(inputFactor);
    if ((max_diameter && bit_diameter >= (*max_diameter).asInch(inputFactor)) ||
        (min_diameter && bit_diameter < (*min_diameter).asInch(inputFactor))) {
      path = holes.erase(path); // remove because it's outside the range.
    } else {
      path++;
    }
  }

  //If the onedrill option has been selected, we can merge all the holes in a single path
  //in order to optimize it even more
  if (onedrill && holes.size() > 0) {
    // Let all drills be the same size as the first drill.
    const auto& first_drill_bit = bits.at(holes.begin()->first);
    for (auto& current_drill : bits) {
      current_drill.second = first_drill_bit;
    }
  }

  //If there are multiple drills with the same size, combine them.
  for (auto bit0 = holes.begin(); bit0 != holes.end(); bit0++) {
    const auto& bit_string0 = drill_to_string(bits.at(bit0->first));
    for (auto bit1 = std::next(bit0); bit1 != holes.end(); bit1++) {
      // If the two bits are the same size, copy the holes to
      // drill from bit1 to bit0.
      const auto& bit_string1 = drill_to_string(bits.at(bit1->first));
      if (bit_string0 == bit_string1) {
        const auto& drill_num0 = bit0->first;
        const auto& drill_num1 = bit1->first;
        holes[drill_num0].insert(holes[drill_num0].end(),
                                 holes[drill_num1].begin(),
                                 holes[drill_num1].end());
        // Remove the holes in bit1.
        holes[drill_num1].clear();
      }
    }
  }

  // Now remove all empty paths.
  for (auto it = holes.begin(); it != holes.end();) {
    if (it->second.empty()) {
      it = holes.erase(it);
    } else {
      it++;
    }
  }

  //Optimize the holes path
  for (auto& path : holes) {
    if (tsp_2opt) {
      tsp_solver::tsp_2opt(path.second, point_type_fp(get_xvalue(0) + xoffset, get_yvalue(0) + yoffset));
    } else {
      tsp_solver::nearest_neighbour(path.second, point_type_fp(get_xvalue(0) + xoffset, get_yvalue(0) + yoffset));
    }
  }

  // Sort the holes in ascending drill size order.
  //  vector<pair<int, multi_linestring_type_fp>> sorted_holes(holes.cbegin(), holes.cend());
  vector<pair<int, multi_linestring_type_fp>> sorted_holes(holes.cbegin(), holes.cend());

  std::sort(sorted_holes.begin(), sorted_holes.end(), [&bits](const auto& a, const auto& b) {
    return bits[a.first].as_length().asInch(1) < bits[b.first].as_length().asInch(1);
  });


  return sorted_holes;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
map<int, drillbit> ExcellonProcessor::optimize_bits() {
  map<int, drillbit> bits(parsed_bits);
  // If there is a list of available bits, round the holes to the nearest
  // available bit.
  if (available_drills.size() > 0) {
    for (auto& wanted_drill : bits) {
      auto& wanted_drill_bit = wanted_drill.second;
      auto old_string = drill_to_string(wanted_drill_bit);
      const Length& wanted_length = wanted_drill_bit.as_length();
      if (min_milldrill_diameter &&
          wanted_length.asInch(inputFactor) >= min_milldrill_diameter->asInch(inputFactor)) {
        // We're not going to drill this one anyway so don't adjust it.
        continue;
      }
      auto best_available_drill = std::min_element(
          available_drills.begin(), available_drills.end(),
          [&](AvailableDrill a, AvailableDrill b) {
            return a.difference(wanted_length, inputFactor).value_or(std::numeric_limits<double>::infinity()) <
                b.difference(wanted_length, inputFactor).value_or(std::numeric_limits<double>::infinity());
          });

      const auto difference = best_available_drill->difference(wanted_length, inputFactor);
      if (difference) {
        wanted_drill_bit.diameter = best_available_drill->diameter().asInch(inputFactor);
        wanted_drill_bit.unit = "inch";
        if (abs(*difference) > 1e-6) {
          cerr << "Info: bit " << wanted_drill.first << " ("
               << old_string << ") is rounded to "
               << drill_to_string(wanted_drill_bit) << std::endl;
        }
      }
    }
  }
  return bits;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::set_preamble(string _preamble)
{
    preamble_ext = _preamble;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::set_postamble(string _postamble)
{
    postamble_ext = _postamble;
}
