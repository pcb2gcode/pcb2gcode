/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others
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
 
#ifndef NGCEXPORTER_H
#define NGCEXPORTER_H

#include <vector>
using std::vector;

#include <string>
using std::string;
using std::pair;

#include <fstream>
using std::ofstream;

#include <memory>
using std::shared_ptr;
using std::dynamic_pointer_cast;

#include <boost/program_options.hpp>

#include "geometry.hpp"
#include "mill.hpp"
#include "exporter.hpp"
#include "unique_codes.hpp"
#include "autoleveller.hpp"
#include "common.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class NGC_Exporter: public Exporter
{
public:
    NGC_Exporter(shared_ptr<Board> board);
    /* virtual void add_path( shared_ptr<icoords> ); */
    /* virtual void add_path( vector< shared_ptr<icoords> > ); */
    void add_header(string);
    void export_all(boost::program_options::variables_map&);
    void set_preamble(string);
    void set_postamble(string);
    inline Tiling::TileInfo getTileInfo()
    {
        return tileInfo;
    }

protected:
  void export_layer(shared_ptr<Layer> layer, string of_name, boost::optional<autoleveller> leveller);
  void cutter_milling(std::ofstream& of, shared_ptr<Cutter> cutter, shared_ptr<icoords> path,
                      const vector<unsigned int>& bridges, double xoffsetTot, double yoffsetTot);

    shared_ptr<Board> board;
    vector<string> header;
    string preamble;        //Preamble from command line (user file)
    string postamble;       //Postamble from command line (user file)

    double cfactor;         //imperial/metric conversion factor for output file
    bool bMetricinput;      //if true, input parameters are in metric units
    bool bMetricoutput;     //if true, metric g-code output
    bool bZchangeG53;
    const unsigned int dpi;
    const double quantization_error;

    bool bTile;

    double xoffset;
    double yoffset;
    
    Tiling::TileInfo tileInfo;
    unsigned int tileXNum;
    unsigned int tileYNum;
    
    uniqueCodes ocodes;
    uniqueCodes globalVars;
};

#endif // NGCEXPORTER_H
