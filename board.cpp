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
Board::Board(int dpi, bool fill_outline, double outline_width, string outputdir) :
    margin(0.0),
    dpi(dpi),
    fill_outline(fill_outline),
    outline_width(outline_width),
    outputdir(outputdir)
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
void Board::prepareLayer(string layername, shared_ptr<LayerImporter> importer, shared_ptr<RoutingMill> manufacturer, bool mirror, bool mirror_absolute)
{
    // see comment for prep_t in board.hpp
    prepared_layers.insert(std::make_pair(layername, make_tuple(importer, manufacturer, mirror, mirror_absolute)));
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

    // calculate room needed by the PCB traces
    for( map< string, prep_t >::iterator it = prepared_layers.begin(); it != prepared_layers.end(); it++ )
    {
        shared_ptr<LayerImporter> importer = it->second.get<0>();
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

    // if there's no pcb outline, add the specified margins
    try
    {
        shared_ptr<RoutingMill> outline_mill = prepared_layers.at("outline").get<1>();
        ivalue_t radius = outline_mill->tool_diameter / 2;
        min_x -= radius;
        max_x += radius;
        min_y -= radius;
        max_y += radius;
    }
    catch( std::logic_error& e )
    {
        try
        {
            shared_ptr<Isolator> trace_mill = boost::static_pointer_cast<Isolator>(prepared_layers.at("front").get<1>());
            ivalue_t radius = trace_mill->tool_diameter / 2;
            int passes = trace_mill->extra_passes + 1;
            min_x -= radius * passes;
            max_x += radius * passes;
            min_y -= radius * passes;
            max_y += radius * passes;
        }
        catch( std::logic_error& e )
        {
            min_x -= margin;
            max_x += margin;
            min_y -= margin;
            max_y += margin;
        }
    }

    min_x -= quantization_error;
    max_x += quantization_error;
    min_y -= quantization_error;
    max_y += quantization_error;

    // board size calculated. create layers
    for( map<string, prep_t>::iterator it = prepared_layers.begin(); it != prepared_layers.end(); it++ )
    {
        // prepare the surface
        shared_ptr<Surface> surface(new Surface(dpi, min_x, max_x, min_y, max_y, outputdir));
        shared_ptr<LayerImporter> importer = it->second.get<0>();
        surface->render(importer);

        shared_ptr<Layer> layer(new Layer(it->first, surface, it->second.get<1>(), it->second.get<2>(), it->second.get<3>())); // see comment for prep_t in board.hpp

        layers.insert(std::make_pair(layer->get_name(), layer));
    }

    // DEBUG output
    BOOST_FOREACH( layer_t layer, layers )
    {
        layer.second->surface->save_debug_image(string("original_") + layer.second->get_name());
    }

    // mask layers with outline
    if (prepared_layers.find("outline") != prepared_layers.end())
    {
        shared_ptr<Layer> outline_layer = layers.at("outline");

        if (fill_outline)
        {
            outline_layer->surface->fill_outline(outline_width);
        }

        for (map<string, shared_ptr<Layer> >::iterator it = layers.begin(); it != layers.end(); it++)
        {
            if (it->second != outline_layer)
            {
                it->second->add_mask(outline_layer);
                it->second->surface->save_debug_image("masked");
            }
        }
    }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
vector<shared_ptr<icoords> > Board::get_toolpath(string layername)
{
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

    BOOST_FOREACH( layer_t layer, layers )
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
