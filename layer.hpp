/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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

#ifndef LAYER_H
#define LAYER_H

#include <string>
#include <vector>

#include <boost/noncopyable.hpp>

#include "geometry.hpp"
#include "surface_vectorial.hpp"
#include "mill.hpp"

class Layer : private boost::noncopyable {
 public:
  Layer(const std::string& name, std::shared_ptr<Surface_vectorial> surface,
        std::shared_ptr<RoutingMill> manufacturer, bool backside);

  std::vector<std::pair<coordinate_type_fp, std::vector<std::shared_ptr<icoords>>>> get_toolpaths();
  std::shared_ptr<RoutingMill> get_manufacturer();
  std::vector<size_t> get_bridges(std::shared_ptr<icoords> toolpath);
  std::string get_name() {
    return name;
  }
  void add_mask(std::shared_ptr<Layer>);

 private:
  std::string name;
  bool mirrored;
  std::shared_ptr<Surface_vectorial> surface;
  std::shared_ptr<RoutingMill> manufacturer;

  friend class Board;
};

#endif // LAYER_H
