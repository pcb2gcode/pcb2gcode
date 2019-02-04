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

#ifndef CORE_H
#define CORE_H

#include <memory>
#include <vector>
#include <string>
#include "geometry.hpp"
#include "mill.hpp"

class Core {
 public:
  // Returns a vector of toolpaths.  A toolpath is a vector of linestrings.  A
  // linestring is a vector of points.  Each point is an x,y pair of doubles.
  virtual std::vector<std::vector<std::shared_ptr<icoords>>> get_toolpath(
      std::shared_ptr<RoutingMill> mill, bool mirror) = 0;
  virtual void save_debug_image(std::string message) = 0;
  virtual ivalue_t get_width_in() = 0;
  virtual ivalue_t get_height_in() = 0;
  virtual void add_mask(std::shared_ptr<Core>) = 0;

  virtual std::vector<size_t> get_bridges(std::shared_ptr<Cutter> cutter, std::shared_ptr<icoords> toolpath);
};

#endif // IMPORTER_H
