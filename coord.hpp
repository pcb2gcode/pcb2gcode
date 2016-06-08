/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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

#ifndef COORD_H
#define COORD_H

#include <vector>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/register/ring.hpp>

typedef int64_t coordinate_type;
typedef double coordinate_type_fp;
typedef std::pair<int, int> coordpair;
typedef std::vector<coordpair> coords;

typedef double ivalue_t;

typedef std::pair<ivalue_t, ivalue_t> icoordpair;
typedef std::vector<icoordpair> icoords;

//Adaptation of icoordpair to Boost Geometry (point)
BOOST_GEOMETRY_REGISTER_POINT_2D(icoordpair, ivalue_t, cs::cartesian, first, second)

// Adaptation of icoords to Boost Geometry (ring)
BOOST_GEOMETRY_REGISTER_RING(icoords)

#endif // COORD_H
