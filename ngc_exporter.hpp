
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

#ifndef NGCEXPORTER_H
#define NGCEXPORTER_H

#include <vector>
using std::vector;

#include <string>
using std::string;
using std::pair;

#include <fstream>
using std::ofstream;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <boost/program_options.hpp>

#include "coord.hpp"
#include "mill.hpp"
#include "exporter.hpp"

class NGC_Exporter : public Exporter
{
public:
	NGC_Exporter( shared_ptr<Board> board );

	/* virtual void add_path( shared_ptr<icoords> ); */
        /* virtual void add_path( vector< shared_ptr<icoords> > ); */

	void add_header( string );
	void export_all( boost::program_options::variables_map& );
	
	void set_preamble(string);
	void set_postamble(string);

protected:
	void export_layer( shared_ptr<Layer> layer, string of_name );

	shared_ptr<Board> board;
	vector<string> header;
	string preamble, postamble;
};

#endif // NGCEXPORTER_H
