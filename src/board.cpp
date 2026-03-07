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
using std::make_shared;
using std::dynamic_pointer_cast;

#include <string>
using std::string;

using std::get;
using std::static_pointer_cast;

#include <utility>
using std::pair;

#include <vector>
using std::vector;

#include <future>
#include <mutex>

#include "bg_operators.hpp"
#include "options.hpp"

typedef pair<string, Layer> layer_t;

namespace {
std::mutex render_log_mutex;
}

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
  return layers.begin()->second.surface.get_width_in();
}

double Board::get_height() {
  if (layers.size() < 1) {
    return 0;
  }
  return layers.begin()->second.surface.get_height_in();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Board::createLayers(std::map<std::string, std::tuple<GerberImporter, std::shared_ptr<RoutingMill>, bool, bool>>&& prepared_layers)
{
    if (!prepared_layers.size())
      return; // Nothing to do.

    // start calculating the minimal board size

    // Calculate the maximum possible room needed by the PCB traces, for tiling later.
    const auto outline = prepared_layers.find("outline");
    bool const has_outline = outline != prepared_layers.cend();
    if (has_outline &&
        (get<0>(outline->second).get_bounding_box().min_corner() <
         get<0>(outline->second).get_bounding_box().max_corner())) {
      shared_ptr<Cutter> outline_mill = static_pointer_cast<Cutter>(get<1>(outline->second));
      const auto& importer = get<0>(outline->second);
      coordinate_type_fp tool_diameter = outline_mill->tool_diameter;
      bounding_box = bg::return_buffer<box_type_fp>(importer.get_bounding_box(), tool_diameter);
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
            auto current_bounding_box = bg::return_buffer<box_type_fp>(importer.get_bounding_box(), extra_passes_margin + trace_mill->offset);
            bg::expand(bounding_box, current_bounding_box);
          }
        }
      }
    }

    // board size calculated. create layers (in parallel)
    std::vector<std::future<pair<string, Layer>>> layer_futures;
    bool multi_threaded = !options::get_vm()["single-thread"].as<bool>();
    for (const auto& prepared_layer : prepared_layers) {
      const auto layer_name = prepared_layer.first;
      layer_futures.push_back(std::async(multi_threaded ? std::launch::async : std::launch::deferred,
        [&prepared_layers, layer_name, fill_outline = fill_outline,
         bounding_box = bounding_box, tsp_2opt = tsp_2opt,
         mill_feed_direction = mill_feed_direction,
         invert_gerbers = invert_gerbers,
         render_paths_to_shapes = render_paths_to_shapes,
         outputdir = outputdir]() {
          // prepare the surface
          auto const& prep = prepared_layers.at(layer_name);
          GerberImporter const& importer = get<0>(prep);
          const bool fill = fill_outline && layer_name == "outline";

          Surface_vectorial surface(
              bounding_box,
              layer_name, outputdir, tsp_2opt,
              mill_feed_direction, invert_gerbers,
              render_paths_to_shapes || (layer_name == "outline"));
          if (fill) {
            surface.enable_filling();
          }
          {
            std::lock_guard<std::mutex> lock(render_log_mutex);
            std::cout << "Rendering " << layer_name << "..." << std::endl;
          }
          surface.render(importer, get<1>(prep)->optimise);
          {
            std::lock_guard<std::mutex> lock(render_log_mutex);
            std::cout << "Layer " << layer_name << " rendered." << std::endl;
          }
          Layer layer(layer_name,
                      std::move(surface),
                      get<1>(prep),
                      get<2>(prep),
                      get<3>(prep));
          return std::make_pair(layer_name, layer);
        }));
    }
    for (auto& f : layer_futures) {
      auto name_and_layer = f.get();
      layers.insert(std::make_pair(name_and_layer.first, name_and_layer.second));
    }

    // DEBUG output
    for (layer_t layer : layers) {
      layer.second.surface.save_debug_image(string("original_") + layer.second.get_name());
    }

    // mask layers with outline
    if (has_outline &&
        (get<0>(outline->second).get_bounding_box().min_corner() <
         get<0>(outline->second).get_bounding_box().max_corner())) {
      Layer outline_layer = layers.at("outline");

      for (auto& layer : layers) {
        if (layer.first != "outline") {
          layer.second.add_mask(outline_layer);
          layer.second.surface.save_debug_image(string("masked_") + layer.second.get_name());
        }
      }
    }
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
Layer& Board::get_layer(string layername)
{
    return layers.at(layername);
}
