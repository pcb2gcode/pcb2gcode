/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 * Copyright (C) 2015 Nicola Corna <nicola@corna.info>
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

#ifndef MILL_H
#define MILL_H

#include <stdint.h>
#include <string.h>

/******************************************************************************/
/*
 */
/******************************************************************************/
class Mill
{
public:
    virtual ~Mill()
    {
    }
    ;

    double feed;
    double vertfeed;
    int speed;
    double zchange;
    double zsafe;
    double zwork;
    double tolerance;
    bool explicit_tolerance;
    bool backside;
    double spinup_time;
    double spindown_time;
    std::string pre_milling_gcode;
    std::string post_milling_gcode;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class RoutingMill: public Mill {
 public:
  // For cutters, there is only one element and the overlap doesn't matter.
  std::vector<std::pair<double, double>> tool_diameters_and_overlaps;
  bool optimise;
  bool eulerian_paths;
  // A fallback for algorithms that don't support extra-passes and overlap.
  double& tool_diameter() {
    return tool_diameters_and_overlaps[0].first;
  }
  // A fallback for algorithms that don't support extra-passes and overlap.
  double overlap_width() const {
    return tool_diameters_and_overlaps[0].second;
  }
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class Isolator: public RoutingMill
{
public:
    int extra_passes;
    bool voronoi;
    bool preserve_thermal_reliefs;
    double isolation_width;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class Cutter: public RoutingMill
{
public:
    bool do_steps;
    double stepsize;
    unsigned int bridges_num;
    double bridges_height;
    double bridges_width;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class Driller: public Mill
{
};

#endif // MILL_H
