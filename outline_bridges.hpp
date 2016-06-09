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

class outline_bridges_exception: virtual std::exception, virtual boost::exception
{
};

class outline_bridges
{
public:
    static vector<unsigned int> makeBridges ( shared_ptr<icoords> &path, unsigned int number, double length );

protected:
    static vector< pair< unsigned int, double > > findLongestSegments ( const shared_ptr<icoords> path, unsigned int number, double length );
    static vector<unsigned int> insertBridges ( shared_ptr<icoords> path, vector< pair< unsigned int, double > > chosenSegments, double length );
    static icoordpair intermediatePoint( icoordpair p0, icoordpair p1, double position );
};

#endif
