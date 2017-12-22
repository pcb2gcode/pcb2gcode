/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2016 Nicola Corna <nicola@corna.info>
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

#ifndef SURFACE_VECTORIAL_H
#define SURFACE_VECTORIAL_H

#include <vector>
#include <list>
#include <forward_list>
#include <map>
#include <algorithm>
using std::vector;
using std::list;
using std::forward_list;
using std::map;
using std::pair;
using std::copy;
using std::swap;

#include <fstream>
using std::ofstream;

#include <memory>
using std::shared_ptr;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_pair;

#include <boost/noncopyable.hpp>

#include "mill.hpp"
#include "gerberimporter.hpp"
#include "core.hpp"
#include "voronoi.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class Surface_vectorial: public Core, virtual public boost::noncopyable
{
public:
    Surface_vectorial(unsigned int points_per_circle, ivalue_t width, ivalue_t height,
                        string name, string outputdir);

    vector<shared_ptr<icoords> > get_toolpath(shared_ptr<RoutingMill> mill,
            bool mirror);
    void save_debug_image(string message);
    void enable_filling();
    void add_mask(shared_ptr<Core> surface);
    void render(shared_ptr<VectorialLayerImporter> importer);
    
    inline ivalue_t get_width_in()
    {
        return width_in;
    }

    inline ivalue_t get_height_in()
    {
        return height_in;
    }
    
protected:
    const unsigned int points_per_circle;
    const ivalue_t width_in;
    const ivalue_t height_in;
    const string name;
    const string outputdir;
    static unsigned int debug_image_index;

    bool fill;

    shared_ptr<multi_polygon_type> vectorial_surface;
    coordinate_type scale;
    box_type bounding_box;
    
    shared_ptr<Surface_vectorial> mask;

private:
    template <typename multi_poly_t, typename multi_linestring_t>
    void multi_poly_to_multi_linestring(const multi_poly_t& mpoly, multi_linestring_t* mls);
    template <typename poly_t, typename multi_linestring_t>
    void poly_to_multi_linestring(const poly_t& poly, multi_linestring_t* mls);
    // Returns the mask if it exists or the convex hull of the vectorial surfaces if not.
    multi_polygon_type get_mask();
    // Convert all the segments into multiple contiguous paths that
    // together use each segment exactly once.  All segments that are
    // outside the mask are not used.
    multi_linestring_type eulerian_paths(const multi_linestring_type& toolpaths);
    // Grow the input linestring by the specified amount and return
    // the result as a list of coordinates around the path.
    vector<coordinate_type> get_pass_offsets(coordinate_type offset, unsigned int total_passes, bool voronoi);
    // Exapnd a shape by an offset and return the new shape.
    template <typename geo_t>
    const multi_polygon_type buffer(const geo_t& poly, coordinate_type offset);
    template <typename multi_linestring_t>
    static vector<shared_ptr<icoords>> mls_to_icoords(const multi_linestring_t& mls, double scale);
    static void merge_near_points(vector<multi_polygon_type>& multi_polys);
    static void merge_near_points(multi_linestring_type& mls);
    // Just like bg::intersection but with rounding instead of truncation.
    static bool intersection(const multi_polygon_type& geo1, const multi_polygon_type& geo2, multi_polygon_type& out);
};

class svg_writer
{
public:
    svg_writer(string filename, unsigned int pixel_per_in, coordinate_type scale, box_type bounding_box);
    template <typename multi_geo_t>
    void add(const multi_geo_t& geometry, double opacity, double which_color);
    template <typename multi_geo_t>
    void add(const multi_geo_t& geometry, double opacity, unsigned int r, unsigned int g, unsigned int b);
    void add(const linestring_type& geometry, double opacity, double which_color);
    void add(const polygon_type& poly, double opacity, double which_color);
    void add(const polygon_type& poly, double opacity, unsigned int r, unsigned int g, unsigned int b);
    // Returns the visually unique color indexed by which_color.
    void get_color(double which_color, unsigned int *red, unsigned int *green, unsigned int *blue);

protected:
    ofstream output_file;
    box_type bounding_box;
    unique_ptr<bg::svg_mapper<point_type> > mapper;
};

#endif // SURFACE_VECTORIAL_H
