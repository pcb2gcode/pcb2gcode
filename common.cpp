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

#include "common.hpp"

#include <fstream>
#include <boost/algorithm/string.hpp>

#include <boost/format.hpp>
using boost::format;

string getSoftwareString( Software software )
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

bool workSide( const boost::program_options::variables_map &options, string type )
{
    const string side = type + "-side";
    const string front = type + "-front";
    
    if( options.count(front) )
        return options[front].as<bool>();
    else
    {
        if( boost::iequals( options[side].as<string>(), "front" ) )
            return true;
        else if ( boost::iequals( options[side].as<string>(), "back" ) )
            return false;
        else
        {
            if( options.count("front") || !options.count("back") )
                return true;    // back+front, front only, <nothing>
            else
                return false;   // back only
        }
    }
}
