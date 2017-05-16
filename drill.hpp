/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 * Copyright (C) 2010 Bernhard Kubicek <kubicek@gmx.at>
 * Copyright (C) 2013 Erik Schuster <erik@muenchen-ist-toll.de>
 * Copyright (C) 2014, 2015 Nicola Corna <nicola@corna.info>
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

#ifndef DRILL_H
#define DRILL_H

#include <map>
using std::map;

#include <string>
using std::string;

#include <list>
using std::list;

#include <vector>
using std::vector;

#include <map>
using std::map;

#include <memory>
using std::shared_ptr;

extern "C" {
#include <gerbv.h>
}

#include "geometry.hpp"

#include <boost/exception/all.hpp>
class drill_exception: virtual std::exception, virtual boost::exception
{
};

#include "mill.hpp"
#include "tile.hpp"
#include "unique_codes.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class drillbit
{
public:
    //Variables, constants, flags...
    double diameter;
    string unit;
    int drill_count;
};

/******************************************************************************/
/*
 Reads Excellon drill files and directly creates RS274-NGC gcode output.
 While we could easily add different input and output formats for the layerfiles
 to pcb2gcode, i've decided to ditch the importer/exporter scheme here.
 We'll very likely not encounter any drill files that gerbv can't read, and
 still rather likely never export to anything other than a ngc g-code file.
 Also, i'm lazy, and if I turn out to be wrong splitting the code won't be much effort anyway.
 */
/******************************************************************************/
class ExcellonProcessor
{
public:
    ExcellonProcessor(const boost::program_options::variables_map& options,
                      const icoordpair min, const icoordpair max);
    ~ExcellonProcessor();
    void add_header(string);
    void set_preamble(string);
    void set_postamble(string);
    void export_ngc(const string of_dir, const string of_name, shared_ptr<Driller> target,
                    bool onedrill, bool nog81, bool zchange_absolute);
    void export_ngc(const string of_dir, const string of_name,shared_ptr<Cutter> target,
                    bool zchange_absolute);
    
    inline void export_svg(const string of_dir)
    {
        save_svg(get_bits(), get_holes(), of_dir);
    }

    shared_ptr< map<int, drillbit> > get_bits();
    shared_ptr< map<int, ilines> > get_holes();

private:
    void parse_holes();
    void parse_bits();
    bool millhole(std::ofstream &of,
                  double start_x, double start_y,
                  double stop_x, double stop_y,
                  shared_ptr<Cutter> cutter, double holediameter);
    double get_xvalue(double);

    shared_ptr< map<int, ilines> > optimise_path( shared_ptr< map<int, ilines> > original_path, bool onedrill );
    shared_ptr<map<int, drillbit> > optimise_bits( shared_ptr<map<int, drillbit> > original_bits, bool onedrill );

    void save_svg(shared_ptr<const map<int, drillbit> > bits, shared_ptr<const map<int, ilines> > holes, const string of_dir);

    const box_type_fp board_dimensions;
    const ivalue_t board_center_x;

    shared_ptr<map<int, drillbit> > bits;
    shared_ptr<map<int, ilines> > holes;
    gerbv_project_t* project;
    vector<string> header;
    string preamble;        //Preamble for output file

    string preamble_ext;    //Preamble from command line (user file)
    string postamble_ext;   //Postamble from command line (user file)
    double cfactor;         //imperial/metric conversion factor for output file
    string zchange;
    const bool drillfront;
    const bool mirror_absolute;
    const bool bMetricOutput;   //Flag to indicate metric output
    const double quantization_error;
    const double xoffset;
    const double yoffset;
    uniqueCodes ocodes;
    uniqueCodes globalVars;
    const Tiling::TileInfo tileInfo;
    Tiling *tiling;
};

#endif // DRILL_H
