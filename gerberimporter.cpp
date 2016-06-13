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

#include <boost/format.hpp>

#include "gerberimporter.hpp"

namespace bg = boost::geometry;

typedef bg::strategy::transform::rotate_transformer<bg::degree, double, 2, 2> rotate_deg;
typedef bg::strategy::transform::translate_transformer<coordinate_type, 2, 2> translate;

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
    char *filename = new char[strlen(cfilename) + 1];
    strcpy(filename, cfilename);

    gerbv_open_layer_from_filename(project, filename);
    delete[] filename;

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
    
    offset *= bg::math::pi<double>() / 180.0;

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

void GerberImporter::draw_rectangle(point_type point1, point_type point2, coordinate_type height, polygon_type& polygon)
{
    const double angle = atan2(point2.y() - point1.y(), point2.x() - point1.x());
    const coordinate_type dx = height / 2 * sin(angle);
    const coordinate_type dy = height / 2 * cos(angle);

    polygon.outer().push_back(point_type(point1.x() + dx, point1.y() - dy));
    polygon.outer().push_back(point_type(point1.x() - dx, point1.y() + dy));
    polygon.outer().push_back(point_type(point2.x() - dx, point2.y() + dy));
    polygon.outer().push_back(point_type(point2.x() + dx, point2.y() - dy));
    polygon.outer().push_back(polygon.outer().front());
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

void GerberImporter::circular_arc(point_type center, coordinate_type radius,
                    double angle1, double angle2, unsigned int circle_points,
                    linestring_type& linestring)
{
    const unsigned int steps = ceil((angle2 - angle1) / (2 * bg::math::pi<double>()) * circle_points);
    const double angle_step = (angle2 - angle1) / steps;
    
    for (unsigned int i = 0; i < steps; i++)
    {
        const double angle = angle1 + i * angle_step;

        linestring.push_back(point_type(cos(angle) * radius + center.x(),
                                        sin(angle) * radius + center.y()));
    }
    
    linestring.push_back(point_type(cos(angle2) * radius + center.x(),
                                    sin(angle2) * radius + center.y()));
}
void GerberImporter::merge_paths(multi_linestring_type &destination, const linestring_type& source)
{
    const size_t source_size = source.size();

    if (!destination.empty())
    {
        linestring_type& last_ls = destination.back();
    
        if (bg::equals(last_ls.back(), source.front()))
        {
            for (size_t i = 0; i < source_size - 1; i++)
                last_ls.push_back(source[i + 1]);
        }
        else if (bg::equals(last_ls.back(), source.back()))
        {
            for (size_t i = 0; i < source_size - 1; i++)
                last_ls.push_back(source[source_size - i - 2]);
        }
        else if (bg::equals(last_ls.front(), source.front()))
        {
            std::reverse(last_ls.begin(), last_ls.end());
            for (size_t i = 0; i < source_size - 1; i++)
                last_ls.push_back(source[i + 1]);
        }
        else if (bg::equals(last_ls.front(), source.back()))
        {
            std::reverse(last_ls.begin(), last_ls.end());
            for (size_t i = 0; i < source_size - 1; i++)
                last_ls.push_back(source[source_size - i - 2]);
        }
        else
        {
            destination.push_back(source);
        }
    }
    else
    {
        destination.push_back(source);
    }
}

shared_ptr<multi_polygon_type> GerberImporter::generate_paths(const map<unsigned int, multi_linestring_type>& paths,
                                                                shared_ptr<multi_polygon_type> input,
                                                                const gerbv_image_t * const gerber,
                                                                double cfactor, unsigned int points_per_circle)
{
    auto output = make_shared<multi_polygon_type>();

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

void GerberImporter::draw_moire(const double * const parameters, unsigned int circle_points,
                                 coordinate_type cfactor, multi_polygon_type& output)
{
    const point_type center (parameters[0] * cfactor, parameters[1] * cfactor);
    auto mpoly1 = make_shared<multi_polygon_type>();
    auto mpoly2 = make_shared<multi_polygon_type>();

    mpoly2->resize(2);

    draw_rectangle(center, parameters[6] * cfactor, parameters[7] * cfactor, 0, 0, (*mpoly2)[0]);
    draw_rectangle(center, parameters[7] * cfactor, parameters[6] * cfactor, 0, 0, (*mpoly2)[1]);
    bg::union_((*mpoly2)[0], (*mpoly2)[1], *mpoly1);

    mpoly2->clear();

    for (unsigned int i = 0; i < parameters[5]; i++)
    {
        const double external_diameter = parameters[2] - (parameters[3] + parameters[4]) * i;
        double internal_diameter = external_diameter - parameters[3];
        polygon_type poly;

        if (external_diameter <= 0)
            break;
        
        if (internal_diameter < 0)
            internal_diameter = 0;

        draw_regular_polygon(center, external_diameter * cfactor, circle_points, 0,
                                internal_diameter * cfactor, circle_points, poly);

        bg::union_(poly, *mpoly1, *mpoly2);
        mpoly1->clear();
        mpoly1.swap(mpoly2);
    }

    output = *mpoly1; 
}

void GerberImporter::draw_thermal(point_type center, coordinate_type external_diameter, coordinate_type internal_diameter,
                                    coordinate_type gap_width, unsigned int circle_points, multi_polygon_type& output)
{
    polygon_type ring;
    polygon_type rect1;
    polygon_type rect2;
    multi_polygon_type cross;
    
    draw_regular_polygon(center, external_diameter, circle_points,
                            0, internal_diameter, circle_points, ring);
    
    draw_rectangle(center, gap_width, external_diameter + 1, 0, 0, rect1);
    draw_rectangle(center, external_diameter + 1, gap_width, 0, 0, rect2);
    bg::union_(rect1, rect2, cross);
    bg::difference(ring, cross, output);
}

void GerberImporter::generate_apertures_map(const gerbv_aperture_t * const apertures[], map<int, multi_polygon_type>& apertures_map, unsigned int circle_points, coordinate_type cfactor)
{
    const point_type origin (0, 0);

    for (int i = 0; i < APERTURE_MAX; i++)
    {
        const gerbv_aperture_t * const aperture = apertures[i];

        if (aperture)
        {
            const double * const parameters = aperture->parameter;
            auto input = make_shared<multi_polygon_type>();
            auto output = make_shared<multi_polygon_type>();

            switch (aperture->type)
            {
                case GERBV_APTYPE_NONE:
                    continue;

                case GERBV_APTYPE_CIRCLE:
                    input->resize(1);
                    draw_regular_polygon(origin,
                                            parameters[0] * cfactor,
                                            circle_points,
                                            parameters[1] * cfactor,
                                            parameters[2] * cfactor,
                                            circle_points,
                                            input->back());
                    break;

                case GERBV_APTYPE_RECTANGLE:
                    input->resize(1);
                    draw_rectangle(origin,
                                    parameters[0] * cfactor,
                                    parameters[1] * cfactor,
                                    parameters[2] * cfactor,
                                    circle_points,
                                    input->back());
                    break;

                case GERBV_APTYPE_OVAL:
                    input->resize(1);
                    draw_oval(origin,
                                parameters[0] * cfactor,
                                parameters[1] * cfactor,
                                parameters[2] * cfactor,
                                circle_points,
                                input->back());
                    break;

                case GERBV_APTYPE_POLYGON:
                    input->resize(1);
                    draw_regular_polygon(origin,
                                            parameters[0] * cfactor,
                                            parameters[1] * cfactor,
                                            parameters[2] * cfactor,
                                            parameters[3] * cfactor,
                                            circle_points,
                                            input->back());
                    break;
                
                case GERBV_APTYPE_MACRO:
                    if (aperture->simplified)
                    {
                        const gerbv_simplified_amacro_t *simplified_amacro = aperture->simplified;

                        while (simplified_amacro)
                        {
                            const double * const parameters = simplified_amacro->parameter;
                            double rotation;
                            int polarity;
                            multi_polygon_type mpoly;
                            multi_polygon_type mpoly_rotated;

                            switch (simplified_amacro->type)
                            {                     
                                case GERBV_APTYPE_NONE:
	                            case GERBV_APTYPE_CIRCLE:
	                            case GERBV_APTYPE_RECTANGLE:
	                            case GERBV_APTYPE_OVAL:
	                            case GERBV_APTYPE_POLYGON:
	                                std::cerr << "Non-macro aperture during macro drawing: skipping" << std::endl;
	                                simplified_amacro = simplified_amacro->next;
	                                continue;
	
	                            case GERBV_APTYPE_MACRO:
	                                std::cerr << "Macro start aperture during macro drawing: skipping" << std::endl;
	                                simplified_amacro = simplified_amacro->next;
	                                continue;
	
	                            case GERBV_APTYPE_MACRO_CIRCLE:
	                                mpoly.resize(1);
	                                draw_regular_polygon(point_type(parameters[2] * cfactor, parameters[3] * cfactor),
	                                                        parameters[1] * cfactor,
	                                                        circle_points,
	                                                        0, 0, 0,
	                                                        mpoly.front());
	                                polarity = parameters[0];
	                                rotation = parameters[4];
	                                break;
	                            
	                            case GERBV_APTYPE_MACRO_OUTLINE:
	                                mpoly.resize(1);
                                    for (unsigned int i = 0; i < round(parameters[1]) + 1; i++)
                                    {
                                        mpoly.front().outer().push_back(point_type(parameters[i * 2 + 2] * cfactor,
                                                                                    parameters [i * 2 + 3] * cfactor));
                                    }
                                    bg::correct(mpoly.front());
	                                polarity = parameters[0];
                                    rotation = parameters[(2 * int(round(parameters[1])) + 4)];
	                                break;
	                            
	                            case GERBV_APTYPE_MACRO_POLYGON:
	                                mpoly.resize(1);
	                                draw_regular_polygon(point_type(parameters[2] * cfactor, parameters[3] * cfactor),
	                                                        parameters[4] * cfactor,
	                                                        parameters[1],
	                                                        0, 0, 0,
	                                                        mpoly.front());
	                                polarity = parameters[0];
	                                rotation = parameters[5];
	                                break;
	                            
	                            case GERBV_APTYPE_MACRO_MOIRE:
                                    draw_moire(parameters, circle_points, cfactor, mpoly);
                                    polarity = 1;
                                    rotation = parameters[8];
	                                break;

	                            case GERBV_APTYPE_MACRO_THERMAL:
	                                draw_thermal(point_type(parameters[0] * cfactor, parameters[1] * cfactor),
	                                                parameters[2] * cfactor,
	                                                parameters[3] * cfactor,
	                                                parameters[4] * cfactor,
	                                                circle_points,
	                                                mpoly);
	                                polarity = 1;
	                                rotation = parameters[5];
	                                break;

	                            case GERBV_APTYPE_MACRO_LINE20:
	                                mpoly.resize(1);
                                    draw_rectangle(point_type(parameters[2] * cfactor, parameters[3] * cfactor),
                                                    point_type(parameters[4] * cfactor, parameters[5] * cfactor),
                                                    parameters[1] * cfactor, mpoly.front());
	                                polarity = parameters[0];
                                    rotation = parameters[6];
	                                break;

	                            case GERBV_APTYPE_MACRO_LINE21:
	                                mpoly.resize(1);
                                    draw_rectangle(point_type(parameters[3] * cfactor, parameters[4] * cfactor),
                                                    parameters[1] * cfactor,
                                                    parameters[2] * cfactor,
                                                    0, 0, mpoly.front());
	                                polarity = parameters[0];
                                    rotation = parameters[5];
	                                break;

	                            case GERBV_APTYPE_MACRO_LINE22:
	                                mpoly.resize(1);
                                    draw_rectangle(point_type((parameters[3] + parameters[1] / 2) * cfactor,
                                                                (parameters[4] + parameters[2] / 2) * cfactor),
                                                    parameters[1] * cfactor,
                                                    parameters[2] * cfactor,
                                                    0, 0, mpoly.front());
	                                polarity = parameters[0];
                                    rotation = parameters[5];
	                                break;

	                            default:
                                    std::cerr << "Unrecognized aperture: skipping" << std::endl;
                                    simplified_amacro = simplified_amacro->next;
                                    continue;
                            }

                            // For Boost.Geometry a positive angle is considered
                            // clockwise, for Gerber is the opposite
                            bg::transform(mpoly, mpoly_rotated, rotate_deg(-rotation));

                            if (polarity == 0)
                                bg::difference(*input, mpoly_rotated, *output);
                            else
                                bg::union_(*input, mpoly_rotated, *output);
                            input->clear();
                            input.swap(output);
                        
                            simplified_amacro = simplified_amacro->next;
                        }
                    }
                    else
                    {
                        std::cerr << "Macro aperture " << i << " is not simplified: skipping" << std::endl;
                        continue;
                    }
                    break;
                
                case GERBV_APTYPE_MACRO_CIRCLE:
	            case GERBV_APTYPE_MACRO_OUTLINE:
	            case GERBV_APTYPE_MACRO_POLYGON:
	            case GERBV_APTYPE_MACRO_MOIRE:
	            case GERBV_APTYPE_MACRO_THERMAL:
	            case GERBV_APTYPE_MACRO_LINE20:
	            case GERBV_APTYPE_MACRO_LINE21:
	            case GERBV_APTYPE_MACRO_LINE22:
	                std::cerr << "Macro aperture during non-macro drawing: skipping" << std::endl;
	                continue;
	            
	            default:
	                std::cerr << "Unrecognized aperture: skipping" << std::endl;
                    continue;
            }

            apertures_map[i] = *input;
        }
    }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr<multi_polygon_type> GerberImporter::render(unsigned int points_per_circle)
{
    map<unsigned int, multi_linestring_type> paths;
    map<int, multi_polygon_type> apertures_map;
    ring_type region;
    coordinate_type cfactor;
    
    auto input = make_shared<multi_polygon_type>();
    auto output = make_shared<multi_polygon_type>();

    bool contour = false;

    gerbv_image_t *gerber = project->file[0]->image;

    if (gerber->netlist->state->unit == GERBV_UNIT_MM)
        cfactor = scale / 25.4;
    else
        cfactor = scale;

    generate_apertures_map(gerber->aperture, apertures_map, points_per_circle, cfactor);

    for (gerbv_net_t *currentNet = gerber->netlist; currentNet; currentNet = currentNet->next){

        const point_type start (currentNet->start_x * cfactor, currentNet->start_y * cfactor);
        const point_type stop (currentNet->stop_x * cfactor, currentNet->stop_y * cfactor);
        const double * const parameters = gerber->aperture[currentNet->aperture]->parameter;
        multi_polygon_type mpoly;

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
                        linestring_type new_segment;
                        
                        new_segment.push_back(start);
                        new_segment.push_back(stop);
                        
                        merge_paths(paths[currentNet->aperture], new_segment);                        
                    }
                    else if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_RECTANGLE)
                    {
                        mpoly.resize(1);
                        linear_draw_rectangular_aperture(start, stop, parameters[0] * cfactor,
                                                parameters[1] * cfactor, mpoly.back().outer());
                        
                        bg::union_(mpoly, *input, *output);
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
                    const auto aperture_mpoly = apertures_map.find(currentNet->aperture);

                    if (aperture_mpoly != apertures_map.end())
                        bg::transform(aperture_mpoly->second, mpoly, translate(stop.x(), stop.y()));
                    else
                        std::cerr << "Macro aperture " << currentNet->aperture <<
                                    " not found in macros list; skipping" << std::endl;

                    bg::union_(mpoly, *input, *output);
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
                        {
                            input = generate_paths(paths, input, gerber, cfactor, points_per_circle);
                            paths.clear();
                            bg::difference(*input, region, *output);
                        }
                        
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
                {
                    input = generate_paths(paths, input, gerber, cfactor, points_per_circle);
                    paths.clear();
                    bg::difference(*input, region, *output);
                }

                input->clear();
                input.swap(output);
            }

            region.clear();
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR ||
                 currentNet->interpolation == GERBV_INTERPOLATION_CCW_CIRCULAR)
        {
            const gerbv_cirseg_t * const cirseg = currentNet->cirseg;
            linestring_type path;

            if (cirseg != NULL)
            {
                circular_arc(point_type(cirseg->cp_x * scale, cirseg->cp_y * scale),
                                cirseg->width * scale / 2,
                                cirseg->angle1 * bg::math::pi<double>() / 180.0,
                                cirseg->angle2 * bg::math::pi<double>() / 180.0,
                                points_per_circle,
                                path);

                if (contour)
                {
                    if (region.empty())
                        std::copy(path.begin(), path.end(), region.end());
                    else
                        std::copy(path.begin() + 1, path.end(), region.end());
                }
                else
                {
                    merge_paths(paths[currentNet->aperture], path);
                }
            }
            else
                std::cout << "Circular arc requested but cirseg == NULL" << std::endl;
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

    output = generate_paths(paths, input, gerber, cfactor, points_per_circle);

    return output;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
GerberImporter::~GerberImporter()
{
    gerbv_destroy_project(project);
}
