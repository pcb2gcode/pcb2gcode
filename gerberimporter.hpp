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

// This class acts like a multi_polygon_type_fp except it internally
// stores the shapes of polygons separately from the linear circular
// lines, which may be used to create shapes by adding thickness or by
// using the lines as an outline to a new shape.  Storing them
// separately allows doing a lazy computation of the entire surface
// and also allows some algorithms to run faster, such as buffer,
// which can work on the lines instead of having to buffer twice.
class shapes_and_lines {
 public:
  shapes_and_lines(bool fill_closed_lines, bool render_paths_to_shapes) :
    fill_closed_lines(fill_closed_lines),
    render_paths_to_shapes(render_paths_to_shapes) {}
  // Get all the shapes.  If render_paths_to_shapes is true then all
  // the linestrings in raw_shapes are also returned.
  multi_polygon_type_fp as_shape() const;
  // Get all the paths.  If render_paths_to_shapes is true then nothing is returned.
  std::map<coordinate_type_fp, multi_linestring_type_fp> as_paths() const;
  void scale(coordinate_type_fp factor);
  std::vector<std::tuple<
                gerbv_layer_t,
                std::vector<multi_polygon_type_fp>,
                std::map<coordinate_type_fp, multi_linestring_type_fp>>> raw_shapes;
 private:
  bool fill_closed_lines;
  bool render_paths_to_shapes;
};

/******************************************************************************/
/*
 Importer for RS274-X Gerber files.

 GerberImporter is using libgerbv and hence features its suberb support for
 different file formats and gerber dialects.
 */
/******************************************************************************/
class GerberImporter {
public:
  GerberImporter();
  bool load_file(const std::string& path);
  virtual ~GerberImporter();

  virtual box_type_fp get_bounding_box() const;

  virtual shapes_and_lines render(
      bool fill_closed_lines,
      bool render_paths_to_shapes,
      unsigned int points_per_circle) const;
  const gerbv_project_t* get_project() const {
    return project;
  }

protected:
  enum Side { FRONT = 0, BACK = 1 } side;

private:
  gerbv_project_t* project;
};

#endif // GERBERIMPORTER_H
