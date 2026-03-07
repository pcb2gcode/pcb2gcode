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
class NGC_Exporter {
public:
    NGC_Exporter(Board&& board);
    void add_header(std::string);
    void export_all(boost::program_options::variables_map&);
    void set_preamble(std::string);
    void set_postamble(std::string);

protected:
  void export_layer(Layer& layer, std::string of_name, boost::optional<autoleveller> leveller,
                    double xoffset, double yoffset);
  void cutter_milling(std::ofstream& of, Cutter const& cutter, const linestring_type_fp& path,
                      const std::vector<size_t>& bridges, const double xoffsetTot, const double yoffsetTot);
  void isolation_milling(std::ofstream& of, RoutingMill const& mill, const linestring_type_fp& path,
                         boost::optional<autoleveller>& leveller, const double xoffsetTot, const double yoffsetTot);

    Board board;
    std::vector<std::string> header;
    std::string preamble;        //Preamble from command line (user file)
    std::string postamble;       //Postamble from command line (user file)

    double cfactor;         //imperial/metric conversion factor for output file
    bool bMetricinput;      //if true, input parameters are in metric units
    bool bMetricoutput;     //if true, metric g-code output
    bool bZchangeG53;
    bool nom6; // missing m6

    bool bTile;

    Tiling::TileInfo tileInfo;
    unsigned int tileXNum;
    unsigned int tileYNum;
};

#endif // NGCEXPORTER_H
