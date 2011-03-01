
/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010, 2011 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others
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

#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <list>
using std::list;
#include <stdexcept>
#include <string>
using std::string;

#include <boost/noncopyable.hpp>

#include <GetPotM>


class options : boost::noncopyable
{
public:
	static void parse( int argc, char** argv );
	static void parse( string file );
	static void check_parameters();

	static bool flag( string name );
	static bool cl_flag( string name );

	static bool have_dbl( string name );
	static double dbl( string name );
	static double dbl( string name, double default_value );

	static bool have_str( string name );
	static string str( string name );
	static string str( string name, string default_value );

	static string help();
private:
	options(){};
	list<GetPot> sources;

	static void check_generic_parameters();
	static void check_milling_parameters();
	static void check_cutting_parameters();
	static void check_drilling_parameters();

	static options& instance();
};


#endif // OPTIONS_HPP
