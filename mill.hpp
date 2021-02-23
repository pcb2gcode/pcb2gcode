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

#include "options.hpp"
#include "units.hpp"

constexpr unsigned int hash(const char *s, int off = 0) {
    return !s[off] ? 5381 : (hash(s, off+1)*33) ^ s[off];
}

// All tools are mills.
class Mill {
 public:
  virtual ~Mill() {}

  virtual bool update(const std::string& key, const std::string& value, double unit) {
    switch (hash(key.c_str())) {
    case hash("drill-feed"):
      vertfeed = parse_unit<Velocity>(value).asInchPerMinute(unit);
      return true;
    case hash("drill-speed"):
      speed = parse_unit<Rpm>(value).asRpm(1);
      return true;
    case hash("zchange"):
      zchange = parse_unit<Length>(value).asInch(unit);
      return true;
    case hash("zsafe"):
      zsafe = parse_unit<Length>(value).asInch(unit);
      return true;
    case hash("zdrill"):
      zwork = parse_unit<Length>(value).asInch(unit);
      return true;
    case hash("spinup-time"):
      spinup_time = parse_unit<Time>(value).asMillisecond(unit);
      return true;
    case hash("spindown-time"):
      spindown_time = parse_unit<Time>(value).asMillisecond(unit);
      return true;
    case hash("name"):
      return true; // Ignore the name.
    default:
      options::maybe_throw("Error: Unrecognized tool option " + key + "=" + value, ERR_INVALIDPARAMETER);
    }
    return false;
  }
  bool operator==(const Mill& other) {
    return vertfeed == other.vertfeed &&
        speed == other.speed &&
        zchange == other.zchange &&
        zsafe == other.zsafe &&
        zwork == other.zwork &&
        tolerance == other.tolerance &&
        explicit_tolerance == other.explicit_tolerance &&
        spinup_time == other.spinup_time &&
        spindown_time == other.spindown_time &&
        pre_milling_gcode == other.pre_milling_gcode &&
        post_milling_gcode == other.post_milling_gcode;
  }

  double vertfeed; // Vertical speed.
  int speed; // Rotational speed.
  double zchange; // Height at which to change tools.
  double zsafe; // Height at which it is always safe to move around.
  double zwork; // Depth at which work is done, whether etching or drilling.
  double tolerance; // small value to use as an epsilon everwhere.
  bool explicit_tolerance; // Should we g64 or not?
  double spinup_time; // How much time to allow the tool to get up to spinning speed?
  double spindown_time; // How much time to allow the tool to stop rotating?
  std::string pre_milling_gcode; // gcode to insert before using this tool
  std::string post_milling_gcode; // gcode to insert after using this tool
};

// A routing mill is one that draws paths, either for cutting the PCB
// or for etching a line in it.
class RoutingMill: public Mill {
 public:
  double feed;  // Horizontal speed.
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

class RangeMill {
 public:
  bool operator==(const RangeMill& other) {
    return tool_diameter == other.tool_diameter &&
        min_diameter == other.min_diameter &&
        max_diameter == other.max_diameter;
  }

  double tool_diameter;
  double min_diameter = -std::numeric_limits<double>::infinity();
  double max_diameter = std::numeric_limits<double>::infinity();
  virtual bool update(const std::string& key, const std::string& value, double unit) {
    switch (hash(key.c_str())) {
    case hash("diameter"):
      tool_diameter = parse_unit<Length>(value).asInch(unit);
      return true;
    case hash("min-diameter"):
      min_diameter = parse_unit<Length>(value).asInch(unit);
      return true;
    case hash("max-diameter"):
      max_diameter = parse_unit<Length>(value).asInch(unit);
      return true;
    }
    return false;
  }
};

// A cutter cuts the PCB.
class Cutter: public RoutingMill {
 public:
  double tool_diameter;
  unsigned int bridges_num;
  double bridges_height;
  double bridges_width;
};

// A cutter for milldrilling holes in the PCB.
class Milldriller: public RoutingMill, public RangeMill {
 public:
  bool operator==(const Milldriller& other) {
    return RoutingMill::operator==(other) &&
        RangeMill::operator==(other);
  }
  virtual bool update(const std::string& key, const std::string& value, double unit) {
    return RoutingMill::update(key, value, unit) || RangeMill::update(key, value, unit);
  }
};

// A driller only plunges and comes back up, for making holes in the
// PCB.
class Driller: public Mill, public RangeMill {
 public:
  bool operator==(const Driller& other) {
    return Mill::operator==(other) &&
        RangeMill::operator==(other);
  }
  virtual bool update(const std::string& key, const std::string& value, double unit) {
    return RangeMill::update(key, value, unit) || Mill::update(key, value, unit);
  }
};

#endif // MILL_H
