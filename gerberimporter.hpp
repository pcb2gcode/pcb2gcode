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

#include <string>
#include <iostream>
#include <utility>
#include <map>

#include "geometry.hpp"

extern "C" {
#include <gerbv.h>
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
  GerberImporter();
  bool load_file(const std::string& path);
  virtual ~GerberImporter();

  virtual gdouble get_min_x() const;
  virtual gdouble get_max_x() const;
  virtual gdouble get_min_y() const;
  virtual gdouble get_max_y() const;

  virtual std::pair<multi_polygon_type_fp, std::map<coordinate_type_fp, multi_linestring_type_fp>> render(
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
