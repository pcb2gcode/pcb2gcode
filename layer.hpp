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
using std::string;
#include <vector>
using std::vector;

#include <boost/noncopyable.hpp>

#include "geometry.hpp"
#include "surface.hpp"
#include "mill.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class Layer: boost::noncopyable
{
public:
    Layer(const string& name, shared_ptr<Core> surface,
          shared_ptr<RoutingMill> manufacturer, bool backside,
          bool mirror_absolute);

    vector<shared_ptr<icoords> > get_toolpaths();
    shared_ptr<RoutingMill> get_manufacturer();
    vector<unsigned int> get_bridges( shared_ptr<icoords> toolpath );
    string get_name()
    {
        return name;
    }
    ;
    void add_mask(shared_ptr<Layer>);

private:
    string name;
    bool mirrored;
    bool mirror_absolute;
    shared_ptr<Core> surface;
    shared_ptr<RoutingMill> manufacturer;

    friend class Board;
};

#endif // LAYER_H
