/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2017 Nicola Corna <nicola@corna.info>
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

#include "core.hpp"
#include <iostream>             // for operator<<, basic_ostream, basic_ostr...
#include "mill.hpp"             // for Cutter
#include "outline_bridges.hpp"  // for makeBridges

using std::cerr;
using std::endl;
using std::vector;
using std::shared_ptr;

vector<size_t> Core::get_bridges(shared_ptr<Cutter> cutter, shared_ptr<icoords> toolpath) {
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

