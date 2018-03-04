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

#include <sstream>
using std::stringstream;

#include <iomanip>
using std::setprecision;
using std::fixed;

#include <boost/format.hpp>
using boost::format;

#include <glibmm/miscutils.h>
using Glib::build_filename;

#include "drill.hpp"
#include "tsp_solver.hpp"
#include "common.hpp"

using std::pair;
using std::make_pair;
using std::max;
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
                                     const icoordpair min,
                                     const icoordpair max)
    : board_dimensions(point_type_fp(min.first, min.second),
                        point_type_fp(max.first, max.second)),
      board_center_x((min.first + max.first) / 2),
      drillfront(workSide(options, "drill")),
      mirror_absolute(options["mirror-absolute"].as<bool>()),
      bMetricOutput(options["metricoutput"].as<bool>()),
      tsp_2opt(options["tsp-2opt"].as<bool>()),
      quantization_error(2.0 / options["dpi"].as<int>()),
      xoffset(options["zero-start"].as<bool>() ? min.first : 0),
      yoffset(options["zero-start"].as<bool>() ? min.second : 0),
      ocodes(1),
      globalVars(100),
      tileInfo( Tiling::generateTileInfo( options, ocodes, max.second - min.second, max.first - min.first ) )
{

    project = gerbv_create_project();

    const char* cfilename = options["drill"].as<string>().c_str();
    char *filename = new char[strlen(cfilename) + 1];
    strcpy(filename, cfilename);

    gerbv_open_layer_from_filename(project, filename);
    delete[] filename;

    if (project->file[0] == NULL)
    {
        throw drill_exception();
    }

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

    tiling = new Tiling( tileInfo, cfactor );
}

/******************************************************************************/
/*
 Destructor
 */
/******************************************************************************/
ExcellonProcessor::~ExcellonProcessor()
{
    gerbv_destroy_project(project);
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
 Recalculates the x-coordinate based on drillfront and mirror_absolute
 drillfront: drill from front side
 mirror_absolute: mirror back side on y-axis
 xvalue: x-coordinate
 returns the recalulated x-coordinate
 */
/******************************************************************************/
double ExcellonProcessor::get_xvalue(double xvalue)
{
    double retval;

    if (drillfront)        //drill from the front, no calculation needed
    {
        retval = xvalue;
    }
    else
    {
        if (mirror_absolute)        //drill from back side, mirrored along y-axis
        {
            retval = (2 * board_dimensions.min_corner().x() - xvalue);
        }
        else          //drill from back side, mirrored along board center
        {
            retval = (2 * board_center_x - xvalue);
        }
    }

    return retval;
}

unique_ptr<icoords> ExcellonProcessor::line_to_holes(const ilinesegment& line, double drill_diameter)
{
    auto start_x = get_xvalue(line.first.first);
    auto start_y = line.first.second;
    auto stop_x = get_xvalue(line.second.first);
    auto stop_y = line.second.second;
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
    unique_ptr<icoords> holes(new icoords());
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
        holes->push_back(icoordpair(x, y));
    }
    return holes;
}

/******************************************************************************/
/*
 Exports the ngc file for drilling
 of_name: output filename
 driller: ...
 onedrill: if true, only the first drill bit is used, the others are skipped
 
 TODO: 1. Optimise the implementation of onedrill by modifying the bits and using the smallest bit only.
       2. Replace the current tiling implementation (gcode repetition) with a subroutine-based solution
 */
/******************************************************************************/
void ExcellonProcessor::export_ngc(const string of_dir, const string of_name,
                                    shared_ptr<Driller> driller, bool onedrill,
                                    bool nog81, bool zchange_absolute)
{
    double xoffsetTot;
    double yoffsetTot;
    stringstream zchange;

    cout << "Exporting drill... ";

    zchange << setprecision(3) << fixed << driller->zchange * cfactor;

    tiling->setGCodeEnd((zchange_absolute ? "G53 " : "") + string("G00 Z") + zchange.str() +
                         " ( All done -- retract )\n" + postamble_ext +
                         "\nM5      (Spindle off.)\nG04 P" +
                         to_string(driller->spindown_time) +
                        "\nM9      (Coolant off.)\n"
                         "M2      (Program end.)\n\n");

    //open output file
    std::ofstream of;
    of.open(build_filename(of_dir, of_name));

    shared_ptr<const map<int, drillbit> > bits = optimise_bits( get_bits(), onedrill );
    shared_ptr<const map<int, ilinesegments> > holes = optimise_path( get_holes(), onedrill );

    //write header to .ngc file
    for (string s : header)
    {
        of << "( " << s << " )" << "\n";
    }

    of << "( Software-independent Gcode )\n";

    if (!onedrill)
    {
        of << "\n( This file uses " << bits->size() << " drill bit sizes. )\n";
        of << "( Bit sizes:";
        for (map<int, drillbit>::const_iterator it = bits->begin();
                it != bits->end(); it++)
        {
            of << " [" << it->second.diameter << it->second.unit << "]";
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
    of << "S" << left << driller->speed << "     (RPM spindle speed.)\n" << "\n";

    //tiling->header( of );     // See TODO #2

    for (map<int, drillbit>::const_iterator it = bits->begin();
            it != bits->end(); it++)
    {
        //if the command line option "onedrill" is given, allow only the inital toolchange
        if ((onedrill == true) && (it != bits->begin()))
        {
            of << "(Drill change skipped. Forced by 'onedrill' option.)\n" << "\n";
        }
        else
        {
            if (zchange_absolute)
                of << "G53 ";
            of << "G00 Z" << driller->zchange * cfactor << " (Retract)\n" << "T"
               << it->first << "\n" << "M5      (Spindle stop.)\n"
               << "G04 P" << driller->spindown_time
               << "(MSG, Change tool bit to drill size " << it->second.diameter
               << " " << it->second.unit << ")\n"
               << "M6      (Tool change.)\n"
               << "M0      (Temporary machine stop.)\n"
               << "M3      (Spindle on clockwise.)\n"
               << "G0 Z" << driller->zsafe * cfactor << "\n"
               << "G04 P" << driller->spinup_time << "\n\n";
        }
        
        if( nog81 )
            of << "F" << driller->feed * cfactor << '\n';
        else
        {
            of << "G81 R" << driller->zsafe * cfactor << " Z"
               << driller->zwork * cfactor << " F" << driller->feed * cfactor << " ";
        }
        
        double drill_diameter = it->second.unit == "mm" ? it->second.diameter / 25.4 : it->second.diameter;
        for( unsigned int i = 0; i < tileInfo.tileY; i++ )
        {
            yoffsetTot = yoffset - i * tileInfo.boardHeight;
            
            for( unsigned int j = 0; j < tileInfo.tileX; j++ )
            {
                xoffsetTot = xoffset - ( i % 2 ? tileInfo.tileX - j - 1 : j ) * tileInfo.boardWidth;

                const ilinesegments drill_coords = holes->at(it->first);
                ilinesegments::const_iterator line_iter = drill_coords.cbegin();

                while (line_iter != drill_coords.cend())
                {
                    unique_ptr<icoords> holes = line_to_holes(*line_iter, drill_diameter);
                    for (auto& hole : *holes)
                    {
                        const auto x = hole.first;
                        const auto y = hole.second;

                        if( nog81 )
                        {
                            of << "G0 X"
                               << ( x - xoffsetTot ) * cfactor
                               << " Y" << ( ( y - yoffsetTot ) * cfactor) << "\n";
                            of << "G1 Z" << driller->zwork * cfactor << '\n';
                                of << "G1 Z" << driller->zsafe * cfactor << '\n';
                        }
                        else
                        {
                            of << "X"
                               << ( x - xoffsetTot )
                                * cfactor
                                   << " Y" << ( ( y - yoffsetTot ) * cfactor) << "\n";
                        }
                    }
                    ++line_iter;
                }
            }
        }
        of << "\n";
    }
    
    //tiling->footer( of ); // See TODO #2
    of << tiling->getGCodeEnd();
    
    of.close();

    save_svg(bits, holes, of_dir);
}

/******************************************************************************/
/*
 *  mill one circle, returns false if tool is bigger than the circle
 */
/******************************************************************************/
bool ExcellonProcessor::millhole(std::ofstream &of, double start_x, double start_y,
                                 double stop_x, double stop_y,
                                 shared_ptr<Cutter> cutter,
                                 double holediameter)
{

    g_assert(cutter);
    double cutdiameter = cutter->tool_diameter;
    bool slot = (start_x != stop_x ||
                 start_y != stop_y);

    if (cutdiameter * 1.001 >= holediameter)         //In order to avoid a "zero radius arc" error
    {
        of << "G0 X" << start_x * cfactor << " Y" << start_y * cfactor << '\n';
        of << "G1 Z" << cutter->zwork * cfactor << '\n';
        if (slot)
        {
            of << "G0 X" << stop_x * cfactor << " Y" << stop_y * cfactor << '\n';
        }
        of << "G0 Z" << cutter->zsafe * cfactor << "\n\n";

        return false;
    }
    else
    {

        double millr = (holediameter - cutdiameter) / 2.;      //mill radius
        double mill_x;
        double mill_y;
        if (slot)
        {
            double delta_x = stop_x - start_x;
            double delta_y = stop_y - start_y;
            double distance = sqrt(delta_x*delta_x + delta_y*delta_y);
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

        // add delta rotated 90 degrees clockwise then normalize to length millr
        double start_targetx = start_x + mill_y;
        double start_targety = start_y - mill_x;
        // add delta rotated 90 degrees counterclockwise then normalize to length millr
        double start2_targetx = start_x - mill_y;
        double start2_targety = start_y + mill_x;
        // add delta rotated 90 degrees counterclockwise then normalize to length millr
        double stop_targetx = stop_x - mill_y;
        double stop_targety = stop_y + mill_x;
        // add delta rotated 90 degrees clockwise then normalize to length millr
        double stop2_targetx = stop_x + mill_y;
        double stop2_targety = stop_y - mill_x;

        of << "G0 X" << start_targetx * cfactor << " Y" << start_targety * cfactor << '\n';

        // Find the largest z_step that divides 0 through z_work into
        // evenly sized passes such that each pass is at most
        // cutter->stepsize in depth.
        unsigned int stepcount = 1;
        if (cutter->do_steps) {
            stepcount = (unsigned int) ceil(abs(cutter->zwork / cutter->stepsize));
        }

        for (unsigned int current_step = 0; current_step < stepcount; current_step++)
        {
            double z = double(current_step+1)/(stepcount) * cutter->zwork;
            of << "G1 Z" << z * cfactor << '\n';
            if (!slot) {
                // Just drill a full-circle.
                of << "G2 "
                   << " X" << start_targetx * cfactor
                   << " Y" << start_targety * cfactor
                   << " I" << (start_x-start_targetx) * cfactor
                   << " J" << (start_y-start_targety) * cfactor << "\n";
            }
            else
            {
                // Draw the first half circle
                of << "G2 X" << start2_targetx * cfactor
                   << " Y" << start2_targety * cfactor
                   << " I" << (start_x-start_targetx) * cfactor
                   << " J" << (start_y-start_targety) * cfactor << "\n";
                // Now across to the second half circle
                of << "G1 X" << stop_targetx * cfactor
                   << " Y" << stop_targety * cfactor << "\n";
                // Draw the second half circle
                of << "G2 X" << stop2_targetx * cfactor
                   << " Y" << stop2_targety * cfactor
                   << " I" << (stop_x-stop_targetx) * cfactor
                   << " J" << (stop_y-stop_targety) * cfactor << "\n";
                // Now back to the start of the first half circle
                of << "G1 X" << start_targetx * cfactor
                   << " Y" << start_targety << "\n";
            }
        }

        of << "G0 Z" << cutter->zsafe * cfactor << "\n\n";

        return true;
    }
}

/******************************************************************************/
/*
 mill larger holes by using a smaller mill-head
 */
/******************************************************************************/
void ExcellonProcessor::export_ngc(const string of_dir, const string of_name,
                                    shared_ptr<Cutter> target, bool zchange_absolute)
{
    unsigned int badHoles = 0;
    double xoffsetTot;
    double yoffsetTot;
    stringstream zchange;

    //g_assert(drillfront == true);       //WHY?
    //g_assert(mirror_absolute == false); //WHY?
    cout << "Exporting drill... " << flush;

    zchange << setprecision(3) << fixed << target->zchange * cfactor;
    tiling->setGCodeEnd((zchange_absolute ? "G53 " : "") + string("G00 Z") + zchange.str() +
                         " ( All done -- retract )\n" + postamble_ext +
                         "\nM5      (Spindle off.)\nG04 P" +
                         to_string(target->spindown_time) +
                        "\nM9      (Coolant off.)\n"
                         "M2      (Program end.)\n\n");

    // open output file
    std::ofstream of;
    of.open(build_filename(of_dir, of_name));

    shared_ptr<const map<int, drillbit> > bits = optimise_bits( get_bits(), false );
    shared_ptr<const map<int, ilinesegments> > holes = optimise_path( get_holes(), false );

    // write header to .ngc file
    for (string s : header)
    {
        of << "( " << s << " )" << "\n";
    }

    if( tileInfo.enabled && tileInfo.software != CUSTOM )
        of << "( Gcode for " << getSoftwareString(tileInfo.software) << " )\n";
    else
        of << "( Software-independent Gcode )\n";

    of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
    of.precision(5);              //Set floating-point decimal precision

    of << "( This file uses a mill head of " << (bMetricOutput ? (target->tool_diameter * 25.4) : target->tool_diameter)
       << (bMetricOutput ? "mm" : "inch") << " to drill the " << bits->size()
       << " bit sizes. )" << "\n";

    of << "( Bit sizes:";
    for (map<int, drillbit>::const_iterator it = bits->begin();
            it != bits->end(); it++)
    {
        of << " [" << it->second.diameter << "]";
    }
    of << " )\n\n";

    //preamble
    of << preamble_ext << preamble << "S" << left << target->speed
       << "    (RPM spindle speed.)\n" << "F" << target->feed * cfactor
       << " (Feedrate)\nM3        (Spindle on clockwise.)\n"
       << "G04 P" << target->spinup_time
       << "\nG00 Z" << target->zsafe * cfactor << "\n\n";

    tiling->header( of );

    for( unsigned int i = 0; i < tileInfo.forYNum; i++ )
    {
        yoffsetTot = yoffset - i * tileInfo.boardHeight;
        
        for( unsigned int j = 0; j < tileInfo.forXNum; j++ )
        {
            xoffsetTot = xoffset - ( i % 2 ? tileInfo.forXNum - j - 1 : j ) * tileInfo.boardWidth;

            if( tileInfo.enabled && tileInfo.software == CUSTOM )
                of << "( Piece #" << j + 1 + i * tileInfo.forXNum << ", position [" << j << ";" << i << "] )\n\n";

            for (map<int, drillbit>::const_iterator it = bits->begin();
                    it != bits->end(); it++)
            {

                double diameter = it->second.unit == "mm" ? it->second.diameter / 25.4 : it->second.diameter;

                const ilinesegments drill_coords = holes->at(it->first);
                ilinesegments::const_iterator line_iter = drill_coords.begin();

                do
                {
                    if( !millhole(of,
                                  get_xvalue(line_iter->first.first) - xoffsetTot,
                                  line_iter->first.second - yoffsetTot,
                                  get_xvalue(line_iter->second.first) - xoffsetTot,
                                  line_iter->second.second - yoffsetTot,
                                  target, diameter) )
                        ++badHoles;

                    ++line_iter;
                }
                while (line_iter != drill_coords.end());
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

    save_svg(bits, holes, of_dir);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::save_svg(shared_ptr<const map<int, drillbit> > bits, shared_ptr<const map<int, ilinesegments> > holes, const string of_dir)
{
    const coordinate_type_fp width = (board_dimensions.max_corner().x() - board_dimensions.min_corner().x()) * SVG_PIX_PER_IN;
    const coordinate_type_fp height = (board_dimensions.max_corner().y() - board_dimensions.min_corner().y()) * SVG_PIX_PER_IN;

    //Some SVG readers does not behave well when viewBox is not specified
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"0 0 %1% %2%\"") % width % height);

    ofstream svg_out (build_filename(of_dir, "original_drill.svg"));
    bg::svg_mapper<point_type_fp> mapper (svg_out, width, height, svg_dimensions);

    mapper.add(board_dimensions);

    for (const pair<int, drillbit>& bit : *bits)
    {
        const ilinesegments drill_lines = holes->at(bit.first);
        const double radius = bit.second.unit == "mm" ?
                              (bit.second.diameter / 25.4) / 2 : bit.second.diameter / 2;

        for (const ilinesegment& line : drill_lines)
        {
            unique_ptr<icoords> holes = line_to_holes(line, radius*2);
            for (auto& hole : *holes)
            {
                mapper.map(hole, "", radius * SVG_PIX_PER_IN);
            }
        }
    }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::parse_bits()
{
    bits = shared_ptr<map<int, drillbit> >(new map<int, drillbit>());

    for (gerbv_drill_list_t* currentDrill = project->file[0]->image->drill_stats
                                            ->drill_list; currentDrill; currentDrill = currentDrill->next)
    {
        drillbit curBit;
        curBit.diameter = currentDrill->drill_size;
        curBit.unit = string(currentDrill->drill_unit);
        curBit.drill_count = currentDrill->drill_count;

        bits->insert(pair<int, drillbit>(currentDrill->drill_num, curBit));
    }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::parse_holes()
{
    if (!bits)
        parse_bits();

    holes = shared_ptr<map<int, ilinesegments> >(new map<int, ilinesegments>());

    for (gerbv_net_t* currentNet = project->file[0]->image->netlist; currentNet;
            currentNet = currentNet->next)
    {
        if (currentNet->aperture != 0)
            (*holes)[currentNet->aperture].push_back(
                ilinesegment(icoordpair(currentNet->start_x, currentNet->start_y),
                             icoordpair(currentNet->stop_x, currentNet->stop_y)));
    }

    for (map<int, drillbit>::iterator it = bits->begin(); it != bits->end(); )
        if (holes->count(it->first) == 0)   //If a bit has no associated holes
        {
			cerr << "Warning: bit " << it->first << " (" << it->second.diameter
				 << ' ' << it->second.unit << ") has no associated holes; "
				 "removing it." << std::endl;
            bits->erase(it++);  //remove it
        }
        else
            ++it;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr< map<int, drillbit> > ExcellonProcessor::get_bits()
{
    if (!bits)
        parse_bits();

    return bits;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr< map<int, ilinesegments> > ExcellonProcessor::get_holes()
{
    if (!holes)
        parse_holes();

    return holes;
}

/******************************************************************************/
/*
 Optimisation of the hole path with a TSP Nearest Neighbour algorithm
 */
/******************************************************************************/
shared_ptr< map<int, ilinesegments> > ExcellonProcessor::optimise_path( shared_ptr< map<int, ilinesegments> > original_path, bool onedrill )
{
    unsigned int size = 0;
    map<int, ilinesegments>::iterator i;

    //If the onedrill option has been selected, we can merge all the holes in a single path
    //in order to optimise it even more
    if( onedrill )
    {
        //First find the total number of holes
        for( i = original_path->begin(); i != original_path->end(); i++ )
            size += i->second.size();

        //Then reserve the vector's size
        original_path->begin()->second.reserve( size );

        //Then copy all the paths inside the first and delete the source vector
        map<int, ilinesegments>::iterator second_element;
        while( original_path->size() > 1 )
        {
            second_element = boost::next( original_path->begin() );
            original_path->begin()->second.insert( original_path->begin()->second.end(),
                                                   second_element->second.begin(), second_element->second.end() );
            original_path->erase( second_element );
        }
    }

    //Otimise the holes path
    for( i = original_path->begin(); i != original_path->end(); i++ )
    {
        if (tsp_2opt) {
            tsp_solver::tsp_2opt( i->second, std::make_pair(get_xvalue(0) + xoffset, yoffset) );
        } else {
            tsp_solver::nearest_neighbour( i->second, std::make_pair(get_xvalue(0) + xoffset, yoffset) );
        }
    }

    return original_path;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr<map<int, drillbit> > ExcellonProcessor::optimise_bits( shared_ptr<map<int, drillbit> > original_bits, bool onedrill )
{
    //The bits optimisation function simply removes all the unnecessary bits when onedrill == true
    if( onedrill )
        original_bits->erase( boost::next( original_bits->begin() ), original_bits->end() );

    return original_bits;
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
