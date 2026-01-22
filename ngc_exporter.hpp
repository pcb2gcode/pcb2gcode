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
#include <string>
#include <fstream>
#include <memory>

#include <boost/program_options.hpp>

#include "geometry.hpp"
#include "mill.hpp"
#include "unique_codes.hpp"
#include "autoleveller.hpp"
#include "common.hpp"
#include "board.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class NGC_Exporter: private boost::noncopyable {
public:
    NGC_Exporter(std::shared_ptr<Board> board);
    void add_header(std::string);
    void export_all(boost::program_options::variables_map&);
    void set_preamble(std::string);
    void set_postamble(std::string);

protected:
  void export_layer(std::shared_ptr<Layer> layer, std::string of_name, boost::optional<autoleveller> leveller);
  void cutter_milling(std::ofstream& of, std::shared_ptr<Cutter> cutter, const linestring_type_fp& path,
                      const std::vector<size_t>& bridges, const double xoffsetTot, const double yoffsetTot);
  void isolation_milling(std::ofstream& of, std::shared_ptr<RoutingMill> mill, const linestring_type_fp& path,
                         boost::optional<autoleveller>& leveller, const double xoffsetTot, const double yoffsetTot);

    std::shared_ptr<Board> board;
    std::vector<std::string> header;
    std::string preamble;        //Preamble from command line (user file)
    std::string postamble;       //Postamble from command line (user file)

    double cfactor;         //imperial/metric conversion factor for output file
    bool bMetricinput;      //if true, input parameters are in metric units
    bool bMetricoutput;     //if true, metric g-code output
    bool bZchangeG53;
    bool nom6; // missing m6
    double min_path_length; // minimum path length to mill (skip Voronoi artifacts)

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
