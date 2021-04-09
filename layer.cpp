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

#include "layer.hpp"

#include <memory>
using std::shared_ptr;
using std::dynamic_pointer_cast;

#include <vector>
using std::vector;

#include <utility>
using std::pair;

#include "outline_bridges.hpp"

#include <iostream>
using std::cerr;
using std::endl;

/******************************************************************************/
/*
 */
/******************************************************************************/
Layer::Layer(const std::string& name, shared_ptr<Surface_vectorial> surface,
             shared_ptr<RoutingMill> manufacturer, bool backside, bool ymirror)
{
    this->name = name;
    this->mirrored = backside;
    this->ymirrored = ymirror;
    this->surface = surface;
    this->manufacturer = manufacturer;
}

#include <iostream>

/******************************************************************************/
vector<pair<coordinate_type_fp, multi_linestring_type_fp>> Layer::get_toolpaths() {
  return surface->get_toolpath(manufacturer, mirrored, ymirrored);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr<RoutingMill> Layer::get_manufacturer()
{
    return manufacturer;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Layer::add_mask(shared_ptr<Layer> mask)
{
    surface->add_mask(mask->surface);
}

vector<size_t> Layer::get_bridges(linestring_type_fp& toolpath) {
  auto cutter = dynamic_pointer_cast<Cutter>(manufacturer);
  auto bridges = outline_bridges::makeBridges(
      toolpath,
      cutter->bridges_num,
      cutter->bridges_width + cutter->tool_diameter );

  return bridges;
}
