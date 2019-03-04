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

#ifndef IMPORTER_H
#define IMPORTER_H

#include <memory>
#include <glibmm/ustring.h>

#include <glibmm/refptr.h>
#include <cairomm/cairomm.h>
#include <gdk/gdkcairo.h>

#include <boost/exception/all.hpp>

#include <utility>
#include <map>

#include "geometry.hpp"
#include "geometry.hpp"

extern "C" {
#include <gerbv.h>
}

/******************************************************************************/
/*
 Pure virtual base class for importers.
 */
/******************************************************************************/
class LayerImporter {
 public:
  virtual gdouble get_min_x() const = 0;
  virtual gdouble get_max_x() const = 0;
  virtual gdouble get_min_y() const = 0;
  virtual gdouble get_max_y() const = 0;
};

class RasterLayerImporter : virtual public LayerImporter {
 public:
  virtual void render(Cairo::RefPtr<Cairo::ImageSurface> surface,
                      const guint dpi, const double xoff, const double yoff,
                      const GdkColor& color =  { 0xFFFFFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
                      const gerbv_render_types_t& renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL) const = 0;

};

class VectorialLayerImporter : virtual public LayerImporter {
 public:
  virtual std::pair<multi_polygon_type_fp, std::map<coordinate_type_fp, multi_linestring_type_fp>> render(
      bool fill_closed_lines,
      bool render_paths_to_shapes,
      unsigned int points_per_circle = 30) const = 0;
};

#endif // IMPORTER_H
