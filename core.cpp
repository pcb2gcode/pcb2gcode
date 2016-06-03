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

#include <iostream>
using std::cerr;
using std::endl;

#include "outline_bridges.hpp"

vector<unsigned int> Core::get_bridges( shared_ptr<Cutter> cutter, shared_ptr<icoords> toolpath )
{
    vector<unsigned int> bridges;

    if( cutter != NULL )
    {
        try
        {
            bridges = outline_bridges::makeBridges( toolpath, 
                                                    cutter->bridges_num,
                                                    cutter->bridges_width + cutter->tool_diameter );

            if ( bridges.size() != cutter->bridges_num )
                cerr << "Can't create " << cutter->bridges_num << " bridges on this layer, "
                     "only " << bridges.size() << " will be created." << endl;
        }
        catch ( outline_bridges_exception &exc )
        {
            cerr << "Can't fit any bridge in the specified outline. Are the bridges are too wide for this outline? "
                 "Are you sure you've selected the correct outline file?" << endl;
        }
    }
    else
        cerr << "Can't create bridges on this layer: cutter object is not castable to shared_ptr<Cutter>" << endl;

    return bridges;
}

