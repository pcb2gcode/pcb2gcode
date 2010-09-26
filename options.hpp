
/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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

#include <stdexcept>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/noncopyable.hpp>

#include <istream>
#include <string>
using std::string;

class options : boost::noncopyable
{
public:
	static void parse_cl( int argc, char** argv );
	static void parse_files();
	static void check_parameters();

	static po::variables_map& get_vm() { return instance().vm; };
	static string help();

private:
	options();
	
	po::variables_map vm;

	po::options_description cli_options; //! CLI options
	po::options_description cfg_options; //! generic options

	static options& instance();
};


#endif // OPTIONS_HPP
