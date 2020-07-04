/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2016 Nicola Corna <nicola@corna.info>
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
 
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/register/ring.hpp>
#include <boost/polygon/polygon.hpp>

// This one chooses the actual size of the output (width and height).
#define SVG_PIX_PER_IN 96
// This one chooses the resolution of the output (viewBox).
#define SVG_DOTS_PER_IN 2000

typedef double coordinate_type_fp;

typedef boost::geometry::model::d2::point_xy<coordinate_type_fp> point_type_fp;
typedef boost::geometry::model::multi_point<point_type_fp> multi_point_type_fp;
typedef boost::geometry::model::segment<point_type_fp> segment_type_fp;
typedef boost::geometry::model::ring<point_type_fp> ring_type_fp;
typedef boost::geometry::model::box<point_type_fp> box_type_fp;
typedef boost::geometry::model::linestring<point_type_fp> linestring_type_fp;
typedef boost::geometry::model::multi_linestring<linestring_type_fp> multi_linestring_type_fp;
typedef boost::geometry::model::polygon<point_type_fp> polygon_type_fp;
typedef boost::geometry::model::multi_polygon<polygon_type_fp> multi_polygon_type_fp;

namespace bg = boost::geometry;

typedef boost::polygon::point_data<coordinate_type_fp> point_type_fp_p;
typedef boost::polygon::segment_data<coordinate_type_fp> segment_type_fp_p;

#endif
