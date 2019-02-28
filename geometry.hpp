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

#include <stdint.h>                                      // for int32_t, int64_t
#include <boost/geometry/geometries/point_xy.hpp>        // for point_xy
#include <boost/geometry/geometries/register/point.hpp>  // for BOOST_GEOMETRY_REGISTER_POINT_2D
#include <boost/geometry/geometries/register/ring.hpp>   // for BOOST_GEOMETRY_REGISTER_RING
#include <utility>                                       // for pair
#include <vector>                                        // for vector
#include "boost/geometry.hpp"                            // for box, linestring, multi_linestring, multi_point, multi_polygon, polygon, ring, segment, cartesian, geometry
#include "boost/polygon/polygon.hpp"                     // for point_data, segment_data

// This one chooses the actual size of the output (width and height).
#define SVG_PIX_PER_IN 96
// This one chooses the resolution of the output (viewBox).
#define SVG_DOTS_PER_IN 2000

typedef int64_t coordinate_type;
typedef double coordinate_type_fp;
typedef int32_t value_t;
typedef double ivalue_t;

typedef std::pair<value_t, value_t> coordpair;
typedef std::vector<coordpair> coords;
typedef std::pair<ivalue_t, ivalue_t> icoordpair;
typedef std::vector<icoordpair> icoords;
typedef std::pair<icoordpair, icoordpair> ilinesegment;
typedef std::vector<ilinesegment> ilinesegments;

//Adaptation of icoordpair to Boost Geometry (point)
BOOST_GEOMETRY_REGISTER_POINT_2D(icoordpair, ivalue_t, cs::cartesian, first, second)

// Adaptation of icoords to Boost Geometry (ring)
BOOST_GEOMETRY_REGISTER_RING(icoords)

typedef boost::geometry::model::d2::point_xy<coordinate_type> point_type;
typedef boost::geometry::model::multi_point<point_type> multi_point_type;
typedef boost::geometry::model::segment<point_type> segment_type;
typedef boost::geometry::model::ring<point_type> ring_type;
typedef boost::geometry::model::box<point_type> box_type;
typedef boost::geometry::model::linestring<point_type> linestring_type;
typedef boost::geometry::model::multi_linestring<linestring_type> multi_linestring_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;
typedef boost::geometry::model::multi_polygon<polygon_type> multi_polygon_type;

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

typedef boost::polygon::point_data<coordinate_type> point_type_p;
typedef boost::polygon::point_data<coordinate_type_fp> point_type_fp_p;
typedef boost::polygon::segment_data<coordinate_type> segment_type_p;
typedef boost::polygon::segment_data<coordinate_type_fp> segment_type_fp_p;

#endif
