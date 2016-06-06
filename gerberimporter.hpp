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

#ifndef GERBERIMPORTER_H
#define GERBERIMPORTER_H

#include <string>
using std::string;

#include "importer.hpp"

using std::make_shared;
using std::map;

extern "C" {
#include <gerbv.h>
}

struct gerber_exception: virtual import_exception
{
};

/******************************************************************************/
/*
 Importer for RS274-X Gerber files.

 GerberImporter is using libgerbv and hence features its suberb support for
 different file formats and gerber dialects.
 
 Vectorial TODO:
  1 - add support for RS274X macros
  2 - add support for region cut-ins 
  3 - test negative regions
 */
/******************************************************************************/
class GerberImporter: public RasterLayerImporter, public VectorialLayerImporter
{
public:
    GerberImporter(const string path);

    virtual gdouble get_width();
    virtual gdouble get_height();
    virtual gdouble get_min_x();
    virtual gdouble get_max_x();
    virtual gdouble get_min_y();
    virtual gdouble get_max_y();

    virtual void render(Cairo::RefPtr<Cairo::ImageSurface> surface,
                        const guint dpi, const double min_x,
                        const double min_y) throw (import_exception);

    virtual shared_ptr<multi_polygon_type> render(unsigned int points_per_circle = 30);
    virtual inline unsigned int vectorial_scale()
    {
        return scale;
    }

    virtual ~GerberImporter();

protected:
    static const unsigned int scale;

    static void draw_regular_polygon(point_type center, coordinate_type diameter, unsigned int vertices,
                            coordinate_type offset, coordinate_type hole_diameter,
                            unsigned int circle_points, polygon_type& polygon);
    
    static void draw_rectangle(point_type center, coordinate_type width, coordinate_type height,
                    coordinate_type hole_diameter, unsigned int circle_points, polygon_type& polygon);

    static void draw_rectangle(point_type point1, point_type point2, coordinate_type height, polygon_type& polygon);
    
    static void draw_oval(point_type center, coordinate_type width, coordinate_type height, coordinate_type hole_diameter,
                unsigned int circle_points, polygon_type& polygon);

    static void draw_thermal(point_type center, coordinate_type external_diameter, coordinate_type internal_diameter,
                coordinate_type gap_width, unsigned int circle_points, multi_polygon_type& output);

    static void draw_moire(const double * const parameters, unsigned int circle_points, coordinate_type cfactor,
                multi_polygon_type& output);

    static void generate_apertures_map(const gerbv_aperture_t * const apertures[],
                map<int, multi_polygon_type>& apertures_map, unsigned int circle_points, coordinate_type cfactor);

    static void linear_draw_rectangular_aperture(point_type startpoint, point_type endpoint, coordinate_type width,
                                coordinate_type height, ring_type& ring);

    static void linear_draw_circular_aperture(point_type startpoint, point_type endpoint,
                                    coordinate_type radius, unsigned int circle_points, ring_type& ring);
    
    //Angles are in rad
    static void circular_arc(point_type center, coordinate_type radius, double angle1,
                                double angle2, unsigned int circle_points, linestring_type& linestring);
    
    static void merge_paths(multi_linestring_type &destination, const linestring_type& source);

    static shared_ptr<multi_polygon_type> generate_paths(const map<unsigned int, multi_linestring_type>& paths,
                                                            shared_ptr<multi_polygon_type> input,
                                                            const gerbv_image_t * const gerber,
                                                            double cfactor, unsigned int points_per_circle);
    
    static void add_region_point(polygon_type& region, const point_type& start, const point_type& stop);

private:

    gerbv_project_t* project;
};

#endif // GERBERIMPORTER_H
