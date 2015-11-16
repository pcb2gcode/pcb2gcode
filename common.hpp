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
 
#ifndef COMMON_H
#define COMMON_H

#include <string>
using std::string;

#include <fstream>

#include <boost/format.hpp>
using boost::format;

// This enum contains the software codes. Note that all the items (except for CUSTOM)
// must start from 0 and be consecutive, as they are used as array indexes
enum Software { CUSTOM = -1, LINUXCNC = 0, MACH4 = 1, MACH3 = 2 };

inline string getSoftwareString( Software software )
{
    switch( software )
    {
        case LINUXCNC:  return "LinuxCNC";
        case MACH4:     return "Mach4";
        case MACH3:     return "Mach3";
        case CUSTOM:    return "custom software";
        default:        return "unknown software";
    }
}

#endif // COMMON_H
