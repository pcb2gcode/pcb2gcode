/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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
 
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#include <stdexcept>
#include <sstream>

#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <tuple>
#include "geometry.hpp"
#include "surface_vectorial.hpp"
#include "layer.hpp"

#include "mill.hpp"

/******************************************************************************/
/*
 Represents a printed circuit board.

 This class calculates the required minimum board size
 and applies the (not yet) described operations on the
 photoplots of each layer to calculate the toolpaths.
 */
/******************************************************************************/
class Board
{
public:
    Board(bool fill_outline,
          std::string outputdir, bool tsp_2opt,
          MillFeedDirection::MillFeedDirection mill_feed_direction, bool invert_gerbers,
          bool render_paths_to_shapes);

    void prepareLayer(std::string layername, std::shared_ptr<GerberImporter> importer,
                      std::shared_ptr<RoutingMill> manufacturer, bool backside);
    void set_margins(double margins) { margin = margins; }
    coordinate_type_fp get_width();
    coordinate_type_fp get_height();
    coordinate_type_fp get_min_x() { return min_x; }
    coordinate_type_fp get_max_x() { return max_x; }
    coordinate_type_fp get_min_y() { return min_y; }
    coordinate_type_fp get_max_y() { return max_y; }
    double get_layersnum() {  return layers.size(); }

    std::vector<std::string> list_layers();
    std::shared_ptr<Layer> get_layer(std::string layername);
    std::vector<std::pair<coordinate_type_fp, multi_linestring_type_fp>> get_toolpath(std::string layername);

    void createLayers(); // should be private

private:
    coordinate_type_fp margin;
    const bool fill_outline;
    const std::string outputdir;
    const bool tsp_2opt;
    const MillFeedDirection::MillFeedDirection mill_feed_direction;
    const bool invert_gerbers;
    const bool render_paths_to_shapes;

    coordinate_type_fp min_x;
    coordinate_type_fp max_x;
    coordinate_type_fp min_y;
    coordinate_type_fp max_y;

    /* The Layer gets constructed from data prepared in
     * prepareLayer after the size calculations are done in createLayers.
     * (It can't be constructed in prepareLayer as the surface which gets
     * passed to the Layer at construction time needs the sizes to be
     * created.)
     * In the meantime, the construction arguments get carried around in
     * prep_t tuples, whose signature must basically match the construction
     * signature of Layer.
     */
    typedef std::tuple<std::shared_ptr<GerberImporter>, std::shared_ptr<RoutingMill>, bool> prep_t;
    std::map<std::string, prep_t> prepared_layers;
    std::map<std::string, std::shared_ptr<Layer> > layers;
};

#endif // BOARD_H
