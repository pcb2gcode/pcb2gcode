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

#include <string>
#include <vector>

// All tools are mills.
class Mill {
 public:
  virtual ~Mill() {}

  double feed;  // Horizontal speed.
  double vertfeed; // Vertical speed.
  int speed; // Rotational speed.
  double zchange; // Height at which to change tools.
  double zsafe; // Height at which it is always safe to move around.
  double zwork; // Height at which milling is done.
  double tolerance; // small value to use as an epsilon everwhere.
  bool explicit_tolerance; // Should we g64 or not?
  bool backside; // Is this tool being used on the back of the board?
  double spinup_time; // How much time to allow the tool to get up to spinnig speed?
  double spindown_time; // How much time to allow the tool to stop rotating?
  std::string pre_milling_gcode; // gcode to insert before using this tool
  std::string post_milling_gcode; // gcode to insert after using this tool
};

// A routing mill is one that draws paths, either for cutting the PCB
// or for etching a line in it.
class RoutingMill: public Mill {
 public:
  double optimise; // Should we optimize this path with Douglas-Peucker algorithm?
  bool eulerian_paths; // Should we combine paths using eulerian walk?
  size_t path_finding_limit; // How much effort to make in path finding?
  double g0_vertical_speed; // How fast is the CNC able to make vertical moves?
  double g0_horizontal_speed; // How fast is the CNC able to make horizontal moves?
  double backtrack; // How much additional milling are we willing to do to save some time?
  double stepsize; // When milling deeply, what is the maximum depth of each pass?
  double offset;  // Stay away from the traces by this amount.
};

// An isolator etches a line into the PCB to separate copper regions.
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

// A cutter cuts the PCB.
class Cutter: public RoutingMill {
 public:
  double tool_diameter;
  unsigned int bridges_num;
  double bridges_height;
  double bridges_width;
};

// A driller only plunges and comes back up, for making holes in the
// PCB.
class Driller: public Mill {

};

#endif // MILL_H
