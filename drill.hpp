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

#ifndef DRILL_H
#define DRILL_H

#include <map>
#include <string>
#include <list>
#include <vector>
#include <memory>

extern "C" {
#include <gerbv.h>
}

#include "geometry.hpp"

#include <boost/exception/all.hpp>
class drill_exception: virtual std::exception, virtual boost::exception
{
};

#include "mill.hpp"
#include "tile.hpp"
#include "unique_codes.hpp"
#include "units.hpp"
#include "available_drills.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class drillbit
{
public:
    //Variables, constants, flags...
    double diameter;
    std::string unit;
    int drill_count;
    Length as_length() const {
        std::ostringstream os;
        os << diameter << unit;
        return parse_unit<Length>(os.str());
    }
};

/******************************************************************************/
/*
 Reads Excellon drill files and directly creates RS274-NGC gcode output.
 While we could easily add different input and output formats for the layerfiles
 to pcb2gcode, i've decided to ditch the importer/exporter scheme here.
 We'll very likely not encounter any drill files that gerbv can't read, and
 still rather likely never export to anything other than a ngc g-code file.
 Also, i'm lazy, and if I turn out to be wrong splitting the code won't be much effort anyway.
 */
/******************************************************************************/
class ExcellonProcessor
{
public:
    ExcellonProcessor(const boost::program_options::variables_map& options,
                      const point_type_fp min, const point_type_fp max);
    void add_header(std::string);
    void set_preamble(std::string);
    void set_postamble(std::string);
    linestring_type_fp line_to_holes(const linestring_type_fp& line, double drill_diameter);
    void export_ngc(const std::string of_dir, const boost::optional<std::string>& of_name,
                    std::shared_ptr<Driller> target, bool onedrill, bool nog81, bool nom6, bool zchange_absolute);
    void export_ngc(const std::string of_dir, const boost::optional<std::string>& of_name,
                    std::shared_ptr<Cutter> target, bool zchange_absolute);

    std::shared_ptr< std::map<int, drillbit> > get_bits();
    std::shared_ptr< std::map<int, multi_linestring_type_fp> > get_holes();

private:
  struct GerbvDeleter {
    void operator()(gerbv_project_t* p) { gerbv_destroy_project(p); }
  };
  std::unique_ptr<gerbv_project_t, GerbvDeleter> parse_project(const std::string& filename);
  std::map<int, drillbit> parse_bits();
  std::map<int, multi_linestring_type_fp> parse_holes();

    bool millhole_one(std::ofstream &of,
                      double start_x, double start_y,
                      double stop_x, double stop_y,
                      std::shared_ptr<Cutter> cutter, double holediameter,
                      bool last_pass = true);
    bool millhole(std::ofstream &of,
                  double start_x, double start_y,
                  double stop_x, double stop_y,
                  std::shared_ptr<Cutter> cutter, double holediameter);
    double get_xvalue(double);
    double get_yvalue(double);
    std::string drill_to_string(drillbit drillbit);

  std::vector<std::pair<int, multi_linestring_type_fp>> optimize_holes(
      std::map<int, drillbit>& bits, bool onedrill,
      const boost::optional<Length>& min_diameter,
      const boost::optional<Length>& max_diameter);
  std::map<int, drillbit> optimize_bits();

    void save_svg(
        const std::map<int, drillbit>& bits,
        const std::vector<std::pair<int, multi_linestring_type_fp>>& holes,
        const std::string& of_dir, const std::string& of_name);

    const box_type_fp board_dimensions;
    const coordinate_type_fp board_center_x;

    std::unique_ptr<gerbv_project_t, GerbvDeleter> const project;
    const bool bMetricOutput;   //Flag to indicate metric output
    const std::map<int, drillbit> parsed_bits;
    const std::map<int, multi_linestring_type_fp> parsed_holes;
    std::vector<std::string> header;
    std::string preamble;        //Preamble for output file

    std::string preamble_ext;    //Preamble from command line (user file)
    std::string postamble_ext;   //Postamble from command line (user file)
    double cfactor;         //imperial/metric conversion factor for output file
    std::string zchange;
    const bool drillfront;
    const double inputFactor;   //Multiply unitless inputs by this value.
    const bool tsp_2opt;        // Perform TSP 2opt optimization on drill path.
    const double xoffset;
    const double yoffset;
    const Length mirror_axis;
    const bool mirror_yaxis;
    // The minimum size hole that is milldrilled, and the minimum entry
    // diameter.  Below this, holes are drilled regularly.
    const boost::optional<Length> min_milldrill_diameter;
    // minimum and maximum entry size and stepover as a fraction of the
    // milldrill cutter size
    const Percent min_milldrill_entry_diameter;
    const Percent max_milldrill_entry_diameter;
    const Percent milldrill_stepover;
    const MillFeedDirection::MillFeedDirection mill_feed_direction;
    const std::vector<AvailableDrill> available_drills;
    uniqueCodes ocodes;
    uniqueCodes globalVars;
    const Tiling::TileInfo tileInfo;
    std::unique_ptr<Tiling> tiling;
};

#endif // DRILL_H
