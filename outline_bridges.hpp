/*
 * This file is part of pcb2gcode.
 *
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

#ifndef OUTLINE_BRIDGES_HPP
#define OUTLINE_BRIDGES_HPP

#include <vector>
using std::vector;
using std::pair;

#include <memory>
using std::shared_ptr;

#include "geometry.hpp"
#include "mill.hpp"

namespace outline_bridges {

/* Insert segments in place, such that the segments are placed in the center of
 * the largest original segments, all of size length.  The path is modified in
 * place.  At most "number" segments are places.  The return is the indicies
 * into the path of the new bridge locations, sorted from smallest to largest
 * index.
 */
vector<size_t> makeBridges(shared_ptr<icoords> &path, size_t number, double length);

}; // namespace outline_bridges

#endif
