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
using std::vector;
using std::pair;

#include <memory>
using std::shared_ptr;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_pair;

#include <boost/noncopyable.hpp>

#include <boost/polygon/polygon.hpp>
#include <boost/polygon/voronoi.hpp>

#include "coord.hpp"
#include "mill.hpp"
#include "gerberimporter.hpp"
#include "core.hpp"

typedef double coordinate_type_fp;

typedef boost::polygon::point_data<coordinate_type_fp> point_type_p;
typedef boost::polygon::segment_data<coordinate_type_fp> segment_type;
typedef boost::polygon::voronoi_builder<coordinate_type> VB;
typedef boost::polygon::voronoi_diagram<coordinate_type_fp> VD;
typedef VD::cell_type cell_type;
typedef VD::cell_type::source_index_type source_index_type;
typedef VD::cell_type::source_category_type source_category_type;
typedef VD::edge_type edge_type;
typedef VD::cell_container_type cell_container_type;
typedef VD::cell_container_type vertex_container_type;
typedef VD::edge_container_type edge_container_type;
typedef VD::const_cell_iterator const_cell_iterator;
typedef VD::const_vertex_iterator const_vertex_iterator;
typedef VD::const_edge_iterator const_edge_iterator;

/******************************************************************************/
/*
 */
/******************************************************************************/
class Surface_vectorial: public Core, virtual public boost::noncopyable
{
public:
    Surface_vectorial(unsigned int points_per_circle);

    vector<shared_ptr<icoords> > get_toolpath(shared_ptr<RoutingMill> mill,
            bool mirror, bool mirror_absolute);
    void save_debug_image(string message);
    void save_debug_image(const multi_polygon_type& mpoly, string message);
    void fill_outline(double linewidth);
    void add_mask(shared_ptr<Core> surface);
    void render(shared_ptr<VectorialLayerImporter> importer);
    
    inline ivalue_t get_width_in()
    {
        return (bounding_box.max_corner().x() - bounding_box.min_corner().x()) / ivalue_t(scale);
    }

    inline ivalue_t get_height_in()
    {
        return (bounding_box.max_corner().y() - bounding_box.min_corner().y()) / ivalue_t(scale);
    }
    
protected:
    const unsigned int points_per_circle;

    shared_ptr<multi_polygon_type> vectorial_surface;
    coordinate_type scale;
    box_type bounding_box;

    static point_type_p retrieve_point(const cell_type& cell, const vector<segment_type> &segments);
    static segment_type retrieve_segment(const cell_type& cell, const vector<segment_type> &segments);
    static void sample_curved_edge(const edge_type *edge, const vector<segment_type> &segments, vector<point_type_p>& sampled_edge, coordinate_type_fp max_dist);
    static void copy_ring(const ring_type& ring, vector<segment_type> &segments);

    static pair<const polygon_type *,ring_type *> find_ring (const multi_polygon_type& input,
                                                             const cell_type& cell, multi_polygon_type& output);

    static void append_remove_extra(ring_type& ring, const point_type point);

    static void build_voronoi(const multi_polygon_type& input, multi_polygon_type &output,
                                coordinate_type bounding_box_offset, coordinate_type max_dist);

    static void offset_polygon(const multi_polygon_type& input, const multi_polygon_type& voronoi,
                            vector< shared_ptr<icoords> >& toolpath, coordinate_type offset,
                            unsigned int points_per_circle, size_t index, unsigned int steps, coordinate_type scale,
                            bool mirror, ivalue_t mirror_axis);

    static inline void push_point(point_type point, bool mirror, coordinate_type mirror_axis,
                                    coordinate_type scale, shared_ptr<icoords> toolpath)
    {
        if (mirror)
            toolpath->push_back(make_pair((2 * mirror_axis - point.x()) / double(scale),
                                            point.y() / double(scale)));
        else
            toolpath->push_back(make_pair(point.x() / double(scale),
                                            point.y() / double(scale)));
    }
    
    template <typename T>
    static bool max_area(T first, T second)
    {
        return boost::geometry::area(first) < boost::geometry::area(second);
    }
};

#endif // SURFACE_VECTORIAL_H
