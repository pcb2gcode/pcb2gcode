/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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

#ifndef GERBERIMPORTER_H
#define GERBERIMPORTER_H

#include "config.h"	// for COMPILE_VALUE_NEW_LINEARX10
#include <string>
#include <iostream>
#include <utility>
#include <map>

#include "geometry.hpp"

extern "C" {
#include <gerbv.h>

// This depends on GERBV_INTERPOLATION_LINEARX10 not being the first item
// in the enum.
#if COMPILE_VALUE_NEW_LINEARX10==0
#      define GERBV_INTERPOLATION_LINEARx10 GERBV_INTERPOLATION_x10
#endif
}

class gerber_exception: public std::exception {};

/******************************************************************************/
/*
 Importer for RS274-X Gerber files.

 GerberImporter is using libgerbv and hence features its suberb support for
 different file formats and gerber dialects.
 */
/******************************************************************************/
class GerberImporter {
public:
  GerberImporter(coordinate_type_fp max_arc_segment_length);
  bool load_file(const std::string& path);
  virtual ~GerberImporter();

  virtual box_type_fp get_bounding_box() const;

  virtual std::pair<multi_polygon_type_fp, std::map<coordinate_type_fp, multi_linestring_type_fp>> render(
      bool fill_closed_lines,
      bool render_paths_to_shapes) const;
  const gerbv_project_t* get_project() const {
    return project;
  }

protected:
  enum Side { FRONT = 0, BACK = 1 } side;

private:
  multi_polygon_type_fp make_circle(point_type_fp center, coordinate_type_fp diameter, coordinate_type_fp offset) const;
  multi_polygon_type_fp make_regular_polygon(point_type_fp center, coordinate_type_fp diameter, unsigned int vertices,
                                             coordinate_type_fp offset, coordinate_type_fp hole_diameter) const;
  multi_polygon_type_fp make_circle(point_type_fp center, coordinate_type_fp diameter,
                                    coordinate_type_fp offset,
                                    coordinate_type_fp hole_diameter) const;
  multi_polygon_type_fp make_rectangle(point_type_fp center, double width, double height,
                                       coordinate_type_fp hole_diameter) const;
  multi_polygon_type_fp make_oval(point_type_fp center, coordinate_type_fp width, coordinate_type_fp height,
                                  coordinate_type_fp hole_diameter) const;
  linestring_type_fp circular_arc(const point_type_fp& start, const point_type_fp& stop,
                                   point_type_fp center, const coordinate_type_fp& radius,
                                   const coordinate_type_fp& radius2, double delta_angle, const bool& clockwise) const;
  multi_polygon_type_fp make_moire(const double * const parameters) const;
  multi_polygon_type_fp make_thermal(point_type_fp center, coordinate_type_fp external_diameter, coordinate_type_fp internal_diameter,
                                   coordinate_type_fp gap_width) const;
  std::map<int, multi_polygon_type_fp> generate_apertures_map(const gerbv_aperture_t * const apertures[]) const;
  coordinate_type_fp const max_arc_segment_length;
  gerbv_project_t* project;
};

#endif // GERBERIMPORTER_H
