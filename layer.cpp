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

#include <memory>                 // for shared_ptr, __shared_ptr_access

#include "layer.hpp"
#include "mill.hpp"               // for Cutter, RoutingMill (ptr only)
#include "surface_vectorial.hpp"  // for Surface_vectorial

using std::shared_ptr;
using std::dynamic_pointer_cast;

#include <vector>                 // for vector

using std::vector;

#include <utility>                // for pair

using std::pair;

#include <iostream>               // for operator<<, basic_ostream, basic_os...

#include "outline_bridges.hpp"    // for makeBridges

using std::cerr;
using std::endl;

/******************************************************************************/
/*
 */
/******************************************************************************/
Layer::Layer(const std::string& name, shared_ptr<Surface_vectorial> surface,
             shared_ptr<RoutingMill> manufacturer, bool backside)
{
    this->name = name;
    this->mirrored = backside;
    this->surface = surface;
    this->manufacturer = manufacturer;
}

/******************************************************************************/
vector<pair<coordinate_type_fp, vector<shared_ptr<icoords>>>> Layer::get_toolpaths() {
  return surface->get_toolpath(manufacturer, mirrored);
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

vector<size_t> Layer::get_bridges( shared_ptr<icoords> toolpath ) {
  auto cutter = dynamic_pointer_cast<Cutter>(manufacturer);
  auto bridges = outline_bridges::makeBridges(
      toolpath,
      cutter->bridges_num,
      cutter->bridges_width + cutter->tool_diameter );

  if (bridges.size() != cutter->bridges_num) {
    cerr << "Can't create " << cutter->bridges_num << " bridges on this layer, "
        "only " << bridges.size() << " will be created." << endl;
  }

  return bridges;
}

