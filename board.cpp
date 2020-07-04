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

#include "board.hpp"
#include <memory>
using std::shared_ptr;
using std::dynamic_pointer_cast;

#include <string>
using std::string;

using std::get;
using std::static_pointer_cast;

#include <utility>
using std::pair;

#include <vector>
using std::vector;

typedef pair<string, shared_ptr<Layer> > layer_t;

/******************************************************************************/
/*
 */
/******************************************************************************/
Board::Board(bool fill_outline, string outputdir, bool tsp_2opt,
             MillFeedDirection::MillFeedDirection mill_feed_direction, bool invert_gerbers,
             bool render_paths_to_shapes) :
    margin(0.0),
    fill_outline(fill_outline),
    outputdir(outputdir),
    tsp_2opt(tsp_2opt),
    mill_feed_direction(mill_feed_direction),
    invert_gerbers(invert_gerbers),
    render_paths_to_shapes(render_paths_to_shapes) {}

double Board::get_width() {
  if (layers.size() < 1) {
    return 0;
  }
  return layers.begin()->second->surface->get_width_in();
}

double Board::get_height() {
  if (layers.size() < 1) {
    return 0;
  }
  return layers.begin()->second->surface->get_height_in();
}

void Board::prepareLayer(string layername, shared_ptr<GerberImporter> importer, shared_ptr<RoutingMill> manufacturer, bool backside) {
  // see comment for prep_t in board.hpp
  prepared_layers.insert(std::make_pair(layername, make_tuple(importer, manufacturer, backside)));
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Board::createLayers()
{
    if (!prepared_layers.size())
      return; // Nothing to do.

    // start calculating the minimal board size

    min_x = INFINITY;
    max_x = -INFINITY;
    min_y = INFINITY;
    max_y = -INFINITY;

    // Calculate the maximum possible room needed by the PCB traces, for tiling later.
    const auto outline = prepared_layers.find("outline");
    if (outline != prepared_layers.cend()) {
      shared_ptr<Cutter> outline_mill = static_pointer_cast<Cutter>(get<1>(outline->second));
      const auto& importer = get<0>(outline->second);
      coordinate_type_fp tool_diameter = outline_mill->tool_diameter;
      min_x = std::min(min_x, importer->get_min_x() - tool_diameter);
      max_x = std::max(max_x, importer->get_max_x() + tool_diameter);
      min_y = std::min(min_y, importer->get_min_y() - tool_diameter);
      max_y = std::max(max_y, importer->get_max_y() + tool_diameter);
    } else {
      for (const auto& layer_name : std::vector<std::string>{"front", "back"}) {
        const auto current_layer = prepared_layers.find(layer_name);
        if (current_layer != prepared_layers.cend()) {
          shared_ptr<Isolator> trace_mill = static_pointer_cast<Isolator>(get<1>(current_layer->second));
          const auto& importer = get<0>(current_layer->second);
          for (const auto& tool : trace_mill->tool_diameters_and_overlap_widths) {
            // Testing showed that 2 was not enough by 3 and above remove all
            // the small connecting lines that would potentially be created.
            double extra_passes_margin = trace_mill->tolerance * 3;
            if (!invert_gerbers) {
              auto tool_diameter = tool.first;
              auto overlap_width = tool.second;
              auto extra_passes = std::max(
                  int(std::ceil((trace_mill->isolation_width - tool_diameter) / (tool_diameter - overlap_width))),
                  trace_mill->extra_passes);
              // Figure out how much margin the extra passes might make.
              extra_passes_margin = tool_diameter + (tool_diameter - overlap_width) * extra_passes;
            }
            min_x = std::min(min_x, importer->get_min_x() - extra_passes_margin - trace_mill->offset);
            max_x = std::max(max_x, importer->get_max_x() + extra_passes_margin + trace_mill->offset);
            min_y = std::min(min_y, importer->get_min_y() - extra_passes_margin - trace_mill->offset);
            max_y = std::max(max_y, importer->get_max_y() + extra_passes_margin + trace_mill->offset);
          }
        }
      }
    }

    // board size calculated. create layers
    for (const auto& prepared_layer : prepared_layers) {
      // prepare the surface
      shared_ptr<GerberImporter> importer = get<0>(prepared_layer.second);
      const bool fill = fill_outline && prepared_layer.first == "outline";

      shared_ptr<Surface_vectorial> surface(new Surface_vectorial(
          30,
          min_x, max_x,
          min_y, max_y,
          prepared_layer.first, outputdir, tsp_2opt,
          mill_feed_direction, invert_gerbers,
          render_paths_to_shapes || (prepared_layer.first == "outline")));
      if (fill) {
        surface->enable_filling();
      }
      surface->render(importer, get<1>(prepared_layer.second)->optimise);
      shared_ptr<Layer> layer(new Layer(prepared_layer.first,
                                        surface,
                                        get<1>(prepared_layer.second),
                                        get<2>(prepared_layer.second))); // see comment for prep_t in board.hpp
      layers.insert(std::make_pair(layer->get_name(), layer));
    }

    // DEBUG output
    for (layer_t layer : layers) {
      layer.second->surface->save_debug_image(string("original_") + layer.second->get_name());
    }

    // mask layers with outline
    if (prepared_layers.find("outline") != prepared_layers.end()) {
      shared_ptr<Layer> outline_layer = layers.at("outline");

      for (const auto& layer : layers) {
        if (layer.second != outline_layer) {
          layer.second->add_mask(outline_layer);
          layer.second->surface->save_debug_image(string("masked_") + layer.second->get_name());
        }
      }
    }
}

vector<pair<coordinate_type_fp, multi_linestring_type_fp>> Board::get_toolpath(string layername) {
  return layers[layername]->get_toolpaths();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
vector<string> Board::list_layers()
{
    vector<string> layerlist;

    for ( layer_t layer : layers )
    {
        layerlist.push_back(layer.first);
    }

    return layerlist;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr<Layer> Board::get_layer(string layername)
{
    return layers.at(layername);
}
