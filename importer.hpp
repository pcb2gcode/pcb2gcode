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

#include <glibmm/ustring.h>
using Glib::ustring;

#include <glibmm/refptr.h>
#include <cairomm/cairomm.h>
#include <gdk/gdkcairo.h>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <boost/exception/all.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include "coord.hpp"

typedef boost::geometry::model::d2::point_xy<coordinate_type> point_type;
typedef boost::geometry::model::ring<point_type> ring_type;
typedef boost::geometry::model::box<point_type> box_type;
typedef boost::geometry::model::linestring<point_type> linestring_type;
typedef boost::geometry::model::multi_linestring<linestring_type> multi_linestring_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;
typedef boost::geometry::model::multi_polygon<polygon_type> multi_polygon_type;

struct import_exception: virtual std::exception, virtual boost::exception
{
};
typedef boost::error_info<struct tag_my_info, ustring> errorstring;

/******************************************************************************/
/*
 Pure virtual base class for importers.
 */
/******************************************************************************/
class LayerImporter
{
public:
    virtual gdouble get_width() = 0;
    virtual gdouble get_height() = 0;
    virtual gdouble get_min_x() = 0;
    virtual gdouble get_max_x() = 0;
    virtual gdouble get_min_y() = 0;
    virtual gdouble get_max_y() = 0;
};

class RasterLayerImporter : virtual public LayerImporter
{
public:
    virtual void render(Cairo::RefPtr<Cairo::ImageSurface> surface,
                        const guint dpi, const double xoff, const double yoff)
    throw (import_exception) = 0;

};

class VectorialLayerImporter : virtual public LayerImporter
{
public:
    virtual shared_ptr<multi_polygon_type> render(unsigned int points_per_circle = 30) = 0;
    virtual unsigned int vectorial_scale() = 0;
};

#endif // IMPORTER_H
