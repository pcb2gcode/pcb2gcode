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

#include <iostream>
#include <algorithm>
#include <utility>
#include <cstdint>

#include <boost/scoped_array.hpp>
#include <boost/format.hpp>

#include <boost/make_shared.hpp>
using boost::make_shared;

#include "gerberimporter.hpp"

namespace bg = boost::geometry;

//As suggested by the Gerber specification, we retain 6 decimals
const unsigned int GerberImporter::scale = 1000000;

/******************************************************************************/
/*
 */
/******************************************************************************/
GerberImporter::GerberImporter(const string path)
{
    project = gerbv_create_project();

    const char* cfilename = path.c_str();
    boost::scoped_array<char> filename(new char[strlen(cfilename) + 1]);
    strcpy(filename.get(), cfilename);

    gerbv_open_layer_from_filename(project, filename.get());
    if (project->file[0] == NULL)
        throw gerber_exception();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_width()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_x - project->file[0]->image->info->min_x;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_height()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_y - project->file[0]->image->info->min_y;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_min_x()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->min_x;

}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_max_x()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_x;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_min_y()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->min_y;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_max_y()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_y;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void GerberImporter::render(Cairo::RefPtr<Cairo::ImageSurface> surface, const guint dpi, const double min_x, const double min_y) throw (import_exception)
{
    gerbv_render_info_t render_info;

    render_info.scaleFactorX = dpi;
    render_info.scaleFactorY = dpi;
    render_info.lowerLeftX = min_x;
    render_info.lowerLeftY = min_y;
    render_info.displayWidth = surface->get_width();
    render_info.displayHeight = surface->get_height();
    render_info.renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL;

    GdkColor color_saturated_white = { 0xFFFFFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
    project->file[0]->color = color_saturated_white;

    cairo_t* cr = cairo_create(surface->cobj());
    gerbv_render_layer_to_cairo_target(cr, project->file[0], &render_info);

    cairo_destroy(cr);

    /// @todo check wheter importing was successful
}

void GerberImporter::draw_regular_polygon(point_type center, coordinate_type diameter, unsigned int vertices,
                            coordinate_type offset, coordinate_type hole_diameter,
                            unsigned int circle_points, polygon_type& polygon)
{
    const double angle_step = 2 * bg::math::pi<double>() / vertices;
    
    offset *= bg::math::d2r;
    
    for (unsigned int i = 0; i < vertices; i++)
        polygon.outer().push_back(point_type(cos(angle_step * i + offset) * diameter / 2 + center.x(),
                               sin(angle_step * i + offset) * diameter / 2 + center.y()));

    if (hole_diameter != 0)
    {
        polygon_type temp_poly;
        
        draw_regular_polygon(center, hole_diameter, circle_points, 0, 0, 0, temp_poly);
        
        polygon.inners().push_back(temp_poly.outer()); 
    }

    bg::correct(polygon);
}

void GerberImporter::draw_rectangle(point_type center, coordinate_type width, coordinate_type height,
                                    coordinate_type hole_diameter, unsigned int circle_points, polygon_type& polygon)
{
    const coordinate_type x = center.x();
    const coordinate_type y = center.y();

    polygon.outer().push_back(point_type(x - width / 2, y - height / 2));
    polygon.outer().push_back(point_type(x - width / 2, y + height / 2));
    polygon.outer().push_back(point_type(x + width / 2, y + height / 2));
    polygon.outer().push_back(point_type(x + width / 2, y - height / 2));
    
    if (hole_diameter != 0)
    {
        polygon_type temp_poly;
        
        draw_regular_polygon(center, hole_diameter, circle_points, 0, 0, 0, temp_poly);
        
        polygon.inners().push_back(temp_poly.outer()); 
    }

    bg::correct(polygon);
}

void GerberImporter::draw_oval(point_type center, coordinate_type width, coordinate_type height,
                                coordinate_type hole_diameter, unsigned int circle_points, polygon_type& polygon)
{
    double angle_step;
    double offset;
    coordinate_type x;
    coordinate_type y;
    coordinate_type radius;
    
    if (circle_points % 2 == 0)
        ++circle_points;
    
    angle_step = 2 * bg::math::pi<double>() / circle_points;
    
    if (width < height)
    {
        radius = width / 2;
        offset = -bg::math::pi<double>();
        x = 0;
        y = height / 2 - radius;
    }
    else
    {
        radius = height / 2;
        offset = bg::math::pi<double>() / 2;
        x = width / 2 - radius;
        y = 0;
    }
    
    for (unsigned int i = 0; i < circle_points / 2 + 1; i++)
        polygon.outer().push_back(point_type(cos(angle_step * i + offset) * radius + center.x() - x,
                                                sin(angle_step * i + offset) * radius + center.y() - y));

    offset += bg::math::pi<double>();

    for (unsigned int i = 0; i < circle_points / 2 + 1; i++)
        polygon.outer().push_back(point_type(cos(angle_step * i + offset) * radius + center.x() + x,
                                                sin(angle_step * i + offset) * radius + center.y() + y));
    
    if (hole_diameter != 0)
    {
        polygon_type temp_poly;
        
        draw_regular_polygon(center, hole_diameter, circle_points, 0, 0, 0, temp_poly);
        
        polygon.inners().push_back(temp_poly.outer()); 
    }
    
    bg::correct(polygon);
}

void GerberImporter::linear_draw_rectangular_aperture(point_type startpoint, point_type endpoint, coordinate_type width,
                                coordinate_type height, ring_type& ring)
{    
    if (startpoint.y() > endpoint.y())
        std::swap(startpoint, endpoint);
    
    if (startpoint.x() > endpoint.x())
    {
        ring.push_back(point_type(startpoint.x() + width / 2, startpoint.y() + height / 2));
        ring.push_back(point_type(startpoint.x() + width / 2, startpoint.y() - height / 2));
        ring.push_back(point_type(startpoint.x() - width / 2, startpoint.y() - height / 2));
        ring.push_back(point_type(endpoint.x() - width / 2, endpoint.y() - height / 2));
        ring.push_back(point_type(endpoint.x() - width / 2, endpoint.y() + height / 2));
        ring.push_back(point_type(endpoint.x() + width / 2, endpoint.y() + height / 2));
    }
    else
    {
        ring.push_back(point_type(startpoint.x() + width / 2, startpoint.y() - height / 2));
        ring.push_back(point_type(startpoint.x() - width / 2, startpoint.y() - height / 2));
        ring.push_back(point_type(startpoint.x() - width / 2, startpoint.y() + height / 2));
        ring.push_back(point_type(endpoint.x() - width / 2, endpoint.y() + height / 2));
        ring.push_back(point_type(endpoint.x() + width / 2, endpoint.y() + height / 2));
        ring.push_back(point_type(endpoint.x() + width / 2, endpoint.y() - height / 2));
    }

    bg::correct(ring);   
}

void GerberImporter::linear_draw_circular_aperture(point_type startpoint, point_type endpoint,
                                    coordinate_type radius, unsigned int circle_points, ring_type& ring)
{
    const coordinate_type dx = endpoint.x() - startpoint.x();
    const coordinate_type dy = endpoint.y() - startpoint.y();
    double angle_step;
    double offset;
    
    if (circle_points % 2 == 0)
        ++circle_points;

    if (startpoint.x() > endpoint.x())
        std::swap(startpoint, endpoint);

    angle_step = 2 * bg::math::pi<double>() / circle_points;
    
    if (dx == 0)
        offset = bg::math::pi<double>();
    else
        offset = atan(dy / dx) + bg::math::pi<double>() / 2;
    
    for (unsigned int i = 0; i < circle_points / 2 + 1; i++)
        ring.push_back(point_type(cos(angle_step * i + offset) * radius + startpoint.x(),
                               sin(angle_step * i + offset) * radius + startpoint.y()));

    offset += bg::math::pi<double>();

    for (unsigned int i = 0; i < circle_points / 2 + 1; i++)
        ring.push_back(point_type(cos(angle_step * i + offset) * radius + endpoint.x(),
                               sin(angle_step * i + offset) * radius + endpoint.y()));
    
    bg::correct(ring);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr<multi_polygon_type> GerberImporter::render(unsigned int points_per_circle)
{
    std::map<unsigned int, multi_linestring_type> paths;
    ring_type region;
    coordinate_type cfactor;
    
    auto input = make_shared<multi_polygon_type>();
    auto output = make_shared<multi_polygon_type>();

    bool contour = false;

    gerbv_image_t *gerber = project->file[0]->image;

    for (unsigned int i = 0; i < APERTURE_MAX; i++) {
        if (gerber->aperture[i] != NULL)
        {
            if (gerber->aperture[i]->type == GERBV_APTYPE_OVAL &&
                gerber->aperture[i]->parameter[0] == gerber->aperture[i]->parameter[1])
            {
                gerber->aperture[i]->type = GERBV_APTYPE_CIRCLE;
            }
        }
    }
    
    if (gerber->netlist->state->unit == GERBV_UNIT_MM)
        cfactor = scale / 25.4;
    else
        cfactor = scale;

    for (gerbv_net_t *currentNet = gerber->netlist; currentNet; currentNet = currentNet->next){

        const point_type start (currentNet->start_x * cfactor, currentNet->start_y * cfactor);
        const point_type stop (currentNet->stop_x * cfactor, currentNet->stop_y * cfactor);
        const double * const parameters = gerber->aperture[currentNet->aperture]->parameter;
        polygon_type temp_poly;

        if (currentNet->interpolation == GERBV_INTERPOLATION_LINEARx1) {
        
            if (currentNet->aperture_state == GERBV_APERTURE_STATE_ON) {
            
                if (contour)
                {
                    if (region.empty())
                        bg::append(region, start);

                    bg::append(region, stop);
                }
                else
                {
                    if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_CIRCLE)
                    {
                        linestring_type temp_ls;
                        multi_linestring_type& current_mls = paths[currentNet->aperture];
                        
                        if (!current_mls.empty())
                        {
                            if (bg::equals(current_mls.back().back(), start))
                            {
                                bg::append(current_mls.back(), stop);
                            }
                            else if (bg::equals(current_mls.back().back(), stop))
                            {
                                bg::append(current_mls.back(), start);
                            }
                            else if (current_mls.back().size() == 2 &&
                                        bg::equals(current_mls.back().front(), start))
                            {
                                std::swap(current_mls.back().front(), current_mls.back().back());
                                bg::append(current_mls.back(), stop);
                            }
                            else if (current_mls.back().size() == 2 &&
                                        bg::equals(current_mls.back().front(), stop))
                            {
                                std::swap(current_mls.back().front(), current_mls.back().back());
                                bg::append(current_mls.back(), start);
                            }
                            else
                            {
                                bg::append(temp_ls, start);
                                bg::append(temp_ls, stop);
                                
                                current_mls.push_back(temp_ls);
                            }
                        }
                        else
                        {
                            bg::append(temp_ls, start);
                            bg::append(temp_ls, stop);
                            
                            current_mls.push_back(temp_ls);
                        }
                    }
                    else if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_RECTANGLE)
                    {
                        linear_draw_rectangular_aperture(start, stop, parameters[0] * cfactor,
                                                parameters[1] * cfactor, temp_poly.outer());
                        
                        bg::union_(temp_poly, *input, *output);
                        input->clear();
                        input.swap(output);
                    }
                    else
                        std::cout << "Error! Drawing with an aperture different from a circle "
                                     "or a rectangle is forbiddden by the Gerber standard; ignoring."
                                  << std::endl;
                }
            }
            
            else if (currentNet->aperture_state == GERBV_APERTURE_STATE_FLASH) {

                if (contour)
                {
                    std::cout << "D03 during contour mode: forbidden by the Gerber standard!" << std::endl;
                }
                else
                {                    
                    switch(gerber->aperture[currentNet->aperture]->type) {
                        case GERBV_APTYPE_NONE:
                            break;

                        case GERBV_APTYPE_CIRCLE:
                            draw_regular_polygon(stop,
                                                    parameters[0] * cfactor,
                                                    points_per_circle,
                                                    parameters[1] * cfactor,
                                                    parameters[2] * cfactor,
                                                    points_per_circle,
                                                    temp_poly);
                            break;

                        case GERBV_APTYPE_RECTANGLE:
                            draw_rectangle(stop,
                                            parameters[0] * cfactor,
                                            parameters[1] * cfactor,
                                            parameters[2] * cfactor,
                                            points_per_circle,
                                            temp_poly);
                            break;

                        case GERBV_APTYPE_OVAL:
                            draw_oval(stop,
                                        parameters[0] * cfactor,
                                        parameters[1] * cfactor,
                                        parameters[2] * cfactor,
                                        points_per_circle,
                                        temp_poly);
                            break;

                        case GERBV_APTYPE_POLYGON:
                            draw_regular_polygon(stop,
                                                    parameters[0] * cfactor,
                                                    parameters[1] * cfactor,
                                                    parameters[2] * cfactor,
                                                    parameters[3] * cfactor,
                                                    points_per_circle,
                                                    temp_poly);
                            break;
                        
                        default:
                            std::cout << "RS274X aperture macros are not supported yet" << std::endl;   //TODO
                    }
                    
                    bg::union_(temp_poly, *input, *output);
                    input->clear();
                    input.swap(output);
                }
            }
            else
            {
                if (contour)
                {
                    if (!region.empty())
                    {
                        bg::append(region, stop);
                        bg::correct(region);

                        if (currentNet->layer->polarity == GERBV_POLARITY_POSITIVE ||
                            currentNet->layer->polarity == GERBV_POLARITY_DARK )
                            bg::union_(*input, region, *output);
                        else
                            bg::difference(*input, region, *output);    //TODO: negative regions have never been tested
                        
                        input->clear();
                        input.swap(output);
                        region.clear();
                    }
                }
            }
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_START)
        {
            contour = true;
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_END)
        {
            contour = false;
            
            if (!region.empty())
            {
                bg::correct(region);
                
                if (currentNet->layer->polarity == GERBV_POLARITY_POSITIVE ||
                    currentNet->layer->polarity == GERBV_POLARITY_DARK )
                    bg::union_(*input, region, *output);
                else
                    bg::difference(*input, region, *output);

                input->clear();
                input.swap(output);
            }

            region.clear();
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR ||
                 currentNet->interpolation == GERBV_INTERPOLATION_CCW_CIRCULAR)
        {
            std::cout << "Circular interpolation mode is not supported yet" << std::endl;   //TODO
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_x10 ||
                 currentNet->interpolation == GERBV_INTERPOLATION_LINEARx01 || 
                 currentNet->interpolation == GERBV_INTERPOLATION_LINEARx001 ) {
            std::cout << "Linear zoomed interpolation modes are not supported "
                         "(are them in the RS274X standard?)" << std::endl;
        }
        else if (currentNet->interpolation != GERBV_INTERPOLATION_DELETED)
        {
            std::cout << "Unrecognized interpolation mode" << std::endl;
        }
    }
    
    for (auto i = paths.begin(); i != paths.end(); i++)
    {
        multi_polygon_type buffered_mls;

        bg::buffer(i->second, buffered_mls,
                   bg::strategy::buffer::distance_symmetric<coordinate_type>
                    (gerber->aperture[i->first]->parameter[0] * cfactor / 2),
                   bg::strategy::buffer::side_straight(),
                   bg::strategy::buffer::join_round(points_per_circle),
                   bg::strategy::buffer::end_round(points_per_circle),
                   bg::strategy::buffer::point_circle(points_per_circle));

        bg::union_(buffered_mls, *input, *output);
        input->clear();
        input.swap(output);
    }
    
    return input;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
GerberImporter::~GerberImporter()
{
    gerbv_destroy_project(project);
}
