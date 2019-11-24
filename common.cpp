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

#include <boost/format.hpp>             // for format

#include "common.hpp"

using boost::format;
using std::string;

#include <boost/system/api_config.hpp>  // for BOOST_POSIX_API

#include "units.hpp"                    // for AUTO, BACK, BoardSide, FRONT

bool workSide( const boost::program_options::variables_map &options, string type )
{
    const string side = type + "-side";
    const string front = type + "-front";
    
    if( options.count(front) )
        return options[front].as<bool>();
    else
    {
        switch (options[side].as<BoardSide::BoardSide>()) {
            case BoardSide::FRONT:
                return true;
            case BoardSide::BACK:
                return false;
            case BoardSide::AUTO:
            default:
                if( options.count("front") || !options.count("back") )
                    return true;    // back+front, front only, <nothing>
                else
                    return false;   // back only
        }
    }
}

// Based on the explanation here:
// https://www.geeksforgeeks.org/python-os-path-join-method/
string build_filename(const string& a, const string& b) {
#ifdef BOOST_WINDOWS_API
  static constexpr auto seperator = '\\';
#endif

#ifdef BOOST_POSIX_API
  static constexpr auto seperator = '/';
#endif

  if (a.size() == 0 || b.front() == seperator) {
    return b;
  }
  if (b.size() == 0) {
    if (a.back() != seperator) {
      return a + seperator;
    } else {
      return a;
    }
  }
  if (a.back() != seperator) {
    return a + seperator + b;
  } else {
    return a + b;
  }
}
