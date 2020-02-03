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
#include <vector>

/******************************************************************************/
/*
 */
/******************************************************************************/
class Mill {
 public:
  virtual ~Mill() {}

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
  double g0_vertical_speed;
  double g0_horizontal_speed;  
  bool toolhead_control;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class RoutingMill: public Mill {
 public:
  bool optimise;
  bool eulerian_paths;
  size_t path_finding_limit;


};

/******************************************************************************/
/*
 */
/******************************************************************************/
class Isolator: public RoutingMill {
 public:
  // Each element is the tool diameter and the overlap width for that tool, both
  // in inches.
  std::vector<std::pair<double, double>> tool_diameters_and_overlap_widths;
  int extra_passes;
  bool voronoi;
  bool preserve_thermal_reliefs;
  double isolation_width;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class Cutter: public RoutingMill {
 public:
  double tool_diameter;
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
