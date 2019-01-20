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

typedef pair<string, shared_ptr<Layer> > layer_t;

/******************************************************************************/
/*
 */
/******************************************************************************/
Board::Board(int dpi, bool fill_outline, double outline_width, string outputdir, bool vectorial, bool tsp_2opt,
             MillFeedDirection::MillFeedDirection mill_feed_direction) :
    margin(0.0),
    dpi(dpi),
    fill_outline(fill_outline),
    outline_width(outline_width),
    outputdir(outputdir),
    vectorial(vectorial),
    tsp_2opt(tsp_2opt),
    mill_feed_direction(mill_feed_direction)
{

}

/******************************************************************************/
/*
 */
/******************************************************************************/
double Board::get_width()
{
    return layers.begin()->second->surface->get_width_in();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
double Board::get_height()
{
    return layers.begin()->second->surface->get_height_in();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
unsigned int Board::get_dpi()
{
    return dpi;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Board::prepareLayer(string layername, shared_ptr<LayerImporter> importer, shared_ptr<RoutingMill> manufacturer, bool backside)
{
    // see comment for prep_t in board.hpp
    prepared_layers.insert(std::make_pair(layername, make_tuple(importer, manufacturer, backside)));
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Board::createLayers()
{
    const double quantization_error = 2.0 / dpi;

    if (!prepared_layers.size())
        throw std::logic_error("No layers prepared.");

    // start calculating the minimal board size

    min_x = INFINITY;
    max_x = -INFINITY;
    min_y = INFINITY;
    max_y = -INFINITY;

    // Calculate the maximum possible room needed by the PCB traces, for tiling later.
    for (map< string, prep_t >::iterator it = prepared_layers.begin(); it != prepared_layers.end(); it++) {
      shared_ptr<LayerImporter> importer = get<0>(it->second);
      float t;
      t = importer->get_min_x();
      if(min_x > t) min_x = t;
      t = importer->get_max_x();
      if(max_x < t) max_x = t;
      t = importer->get_min_y();
      if(min_y > t) min_y = t;
      t = importer->get_max_y();
      if(max_y < t) max_y = t;
    }

    // Calculate the maximum possible margin needed.
    double margin = quantization_error;
    try {
      shared_ptr<RoutingMill> outline_mill = get<1>(prepared_layers.at("outline"));
      ivalue_t tool_diameter = outline_mill->tool_diameter();
      if (tool_diameter > margin) {
        margin = tool_diameter;  // We'll need to make space enough for the cutter to go around.
      }
    } catch (std::logic_error& e) {}
    for (const auto& layer_name : std::vector<std::string>{"front"}) {
      try {
        shared_ptr<Isolator> trace_mill = static_pointer_cast<Isolator>(get<1>(prepared_layers.at(layer_name)));
        for (const auto& diameter_and_overlap : trace_mill->tool_diameters_and_overlaps) {
          ivalue_t tool_diameter = diameter_and_overlap.first;
          ivalue_t overlap_width = diameter_and_overlap.second;
          // Figure out how much margin the extra passes might make.
          double extra_passes_margin = tool_diameter + (tool_diameter - overlap_width) * trace_mill->extra_passes;
          if (extra_passes_margin > margin) {
            margin = extra_passes_margin;
          }
          double isolation_margin = tool_diameter;
          while (isolation_margin < trace_mill->isolation_width) {
            // Add passes until we reach the isolation_width.
            isolation_margin += tool_diameter - overlap_width;
          }
          if (isolation_margin > margin) {
            margin = isolation_margin;
          }
        }
      } catch (std::logic_error& e) {}
    }
    min_x -= margin;
    max_x += margin;
    min_y -= margin;
    max_y += margin;

    // board size calculated. create layers
    for( map<string, prep_t>::iterator it = prepared_layers.begin(); it != prepared_layers.end(); it++ )
    {
        // prepare the surface
        shared_ptr<LayerImporter> importer = get<0>(it->second);
        const bool fill = fill_outline && it->first == "outline";

        if (vectorial)
        {
            if (dynamic_pointer_cast<VectorialLayerImporter>(importer))
            {
                shared_ptr<Surface_vectorial> surface(new Surface_vectorial(30, max_x - min_x,
                                                                            max_y - min_y,
                                                                            it->first, outputdir, tsp_2opt,
                                                                            mill_feed_direction));
                if (fill)
                    surface->enable_filling();

                surface->render(dynamic_pointer_cast<VectorialLayerImporter>(importer));

                shared_ptr<Layer> layer(new Layer(it->first,
                                                    surface,
                                                    get<1>(it->second),
                                                    get<2>(it->second))); // see comment for prep_t in board.hpp

                layers.insert(std::make_pair(layer->get_name(), layer));
            }
            else
                throw std::logic_error("Can't cast LayerImporter to VectorialLayerImporter!");
        }
        else
        {
            if (dynamic_pointer_cast<RasterLayerImporter>(importer))
            {
                shared_ptr<Surface> surface(new Surface(dpi, min_x, max_x, min_y, max_y,
                                                        it->first, outputdir, tsp_2opt));
                if (fill)
                    surface->enable_filling(outline_width);

                surface->render(dynamic_pointer_cast<RasterLayerImporter>(importer));

                shared_ptr<Layer> layer(new Layer(it->first,
                                                    surface, 
                                                    get<1>(it->second),
                                                    get<2>(it->second))); // see comment for prep_t in board.hpp
                
                layers.insert(std::make_pair(layer->get_name(), layer));
            }
            else
                throw std::logic_error("Can't cast LayerImporter to RasterLayerImporter!");
        }
    }

    // DEBUG output
    for ( layer_t layer : layers )
    {
        layer.second->surface->save_debug_image(string("original_") + layer.second->get_name());
    }

    // mask layers with outline
    if (prepared_layers.find("outline") != prepared_layers.end())
    {
        shared_ptr<Layer> outline_layer = layers.at("outline");

        for (map<string, shared_ptr<Layer> >::iterator it = layers.begin(); it != layers.end(); it++)
        {
            if (it->second != outline_layer)
            {
                it->second->add_mask(outline_layer);
                it->second->surface->save_debug_image(string("masked_") + it->second->get_name());
            }
        }
    }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
vector<vector<shared_ptr<icoords>>> Board::get_toolpath(string layername) {
    vector<shared_ptr<icoords> > toolpath;

    try
    {
        return layers[layername]->get_toolpaths();
    }
    catch (std::logic_error& e)
    {
        std::stringstream msg;
        msg << "class Board: get_toolpath(): layer not available: ";
        msg << layername << std::endl;
        throw std::logic_error(msg.str());
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
shared_ptr<Layer> Board::get_layer(string layername)
{
    return layers.at(layername);
}
