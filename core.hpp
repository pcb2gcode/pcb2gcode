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
using std::shared_ptr;

#include <vector>
using std::vector;

#include <string>
using std::string;

#include "geometry.hpp"
#include "mill.hpp"

/******************************************************************************/
/*
 Pure virtual base class for cores.
 */
/******************************************************************************/
class Core
{
public:
    virtual vector<shared_ptr<icoords> > get_toolpath(shared_ptr<RoutingMill> mill,
            bool mirror) = 0;
    virtual void save_debug_image(string message) = 0;
    virtual ivalue_t get_width_in() = 0;
    virtual ivalue_t get_height_in() = 0;
    virtual void add_mask(shared_ptr<Core>) = 0;
    
    virtual vector<size_t> get_bridges(shared_ptr<Cutter> cutter, shared_ptr<icoords> toolpath);
};

#endif // IMPORTER_H
