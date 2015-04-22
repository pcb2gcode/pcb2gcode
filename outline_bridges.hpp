/*!\defgroup OUTLINE_BRIDGES*/
/******************************************************************************/
/*!
 \file       outline_bridges.cpp
 \brief

 \version
 09.02.2015 - Nicola Corna - nicola@corna.info

 \copyright  pcb2gcode is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 pcb2gcode is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.

 \ingroup    SURFACE
 */
/******************************************************************************/

#ifndef OUTLINE_BRIDGES_H
#define OUTLINE_BRIDGES_H

#include <vector>
using std::vector;
using std::pair;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include "coord.hpp"
#include "mill.hpp"

class outline_bridges_exception: virtual std::exception, virtual boost::exception {
};

class outline_bridges {
    public:
        static vector<unsigned int> makeBridges ( shared_ptr<icoords> &path, unsigned int number, double length );
        
    protected:
        static vector< pair< unsigned int, double > > findLongestSegments ( const shared_ptr<icoords> path, unsigned int number, double length );
        static vector<unsigned int> insertBridges ( shared_ptr<icoords> path, vector< pair< unsigned int, double > > chosenSegments, double length );
        static double pointDistance( icoordpair p0, icoordpair p1 );
        static icoordpair intermediatePoint( icoordpair p0, icoordpair p1, double position );
};

#endif
