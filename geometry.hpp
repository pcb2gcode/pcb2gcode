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

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon/point.hpp>
#include <boost/polygon/polygon.hpp>

#include "coord.hpp"

typedef boost::polygon::point_data<coordinate_type> point_type;
typedef boost::polygon::point_data<coordinate_type_fp> point_type_fp;

typedef boost::polygon::segment_data<coordinate_type> segment_type; //Not adapted for Boost.Geometry
typedef boost::polygon::segment_data<coordinate_type_fp> segment_type_fp;   //Not adapted for Boost.Geometry

typedef boost::geometry::model::ring<point_type> ring_type;
typedef boost::geometry::model::box<point_type> box_type;
typedef boost::geometry::model::linestring<point_type> linestring_type;
typedef boost::geometry::model::multi_linestring<linestring_type> multi_linestring_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;
typedef boost::geometry::model::multi_polygon<polygon_type> multi_polygon_type;

#endif
