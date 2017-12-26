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
 
#ifndef VORONOI_H
#define VORONOI_H

#include <boost/polygon/voronoi.hpp>
#include <boost/optional.hpp>

#include <vector>
using std::vector;
using std::pair;

#include <memory>
using std::unique_ptr;

#include "geometry.hpp"

namespace boost { namespace polygon { namespace detail {

template <>
struct voronoi_ctype_traits<int64_t> {
    typedef int64_t int_type;
    typedef extended_int<3> int_x2_type;
    typedef extended_int<3> uint_x2_type;
    typedef extended_int<128> big_int_type;
    typedef fpt64 fpt_type;
    typedef extended_exponent_fpt<fpt_type> efpt_type;
    typedef ulp_comparison<fpt_type> ulp_cmp_type;
    typedef type_converter_fpt to_fpt_converter_type;
    typedef type_converter_efpt to_efpt_converter_type;
};

} } }

typedef boost::polygon::point_data<coordinate_type> point_type_p;
typedef boost::polygon::point_data<coordinate_type_fp> point_type_fp_p;
typedef boost::polygon::segment_data<coordinate_type> segment_type_p;
typedef boost::polygon::segment_data<coordinate_type_fp> segment_type_fp_p;

typedef boost::polygon::voronoi_builder<coordinate_type> voronoi_builder_type;
typedef boost::polygon::voronoi_diagram<coordinate_type_fp> voronoi_diagram_type;

typedef voronoi_diagram_type::cell_type cell_type;
typedef voronoi_diagram_type::cell_type::source_index_type source_index_type;
typedef voronoi_diagram_type::cell_type::source_category_type source_category_type;
typedef voronoi_diagram_type::edge_type edge_type;
typedef voronoi_diagram_type::vertex_type vertex_type;
typedef voronoi_diagram_type::cell_container_type cell_container_type;
typedef voronoi_diagram_type::cell_container_type vertex_container_type;
typedef voronoi_diagram_type::edge_container_type edge_container_type;
typedef voronoi_diagram_type::const_cell_iterator const_cell_iterator;
typedef voronoi_diagram_type::const_vertex_iterator const_vertex_iterator;
typedef voronoi_diagram_type::const_edge_iterator const_edge_iterator;

class Voronoi
{
public:
    /* The returned rings are loops around the input polygons that
     * follow the voronoi diagram.  The loops might extend outside the
     * bounding_box provided.  The loops don't overlap and will
     * together cover at least the entire bounding_box.
     */
    static multi_polygon_type_fp get_voronoi_polygons(
        const multi_polygon_type& input,
        const box_type& bounding_box, coordinate_type max_dist);

protected:
    static linestring_type_fp edge_to_linestring(const edge_type& edge, const vector<segment_type_p>& segments, const box_type_fp& bounding_box, coordinate_type max_dist);
    static void copy_ring(const ring_type& ring, vector<segment_type_p> &segments);
    static point_type_p retrieve_point(const cell_type& cell, const vector<segment_type_p> &segments);
    static const segment_type_p& retrieve_segment(const cell_type& cell, const vector<segment_type_p> &segments);
    static void sample_curved_edge(const edge_type *edge, const vector<segment_type_p> &segments,
                                   vector<point_type_fp_p>& sampled_edge, coordinate_type_fp max_dist);
    static void clip_infinite_edge(
        const edge_type& edge, const vector<segment_type_p>& segments, std::vector<point_type_fp_p>* clipped_edge, const box_type_fp& bounding_box);
    // Do these edges belong to the same polygon?
    static bool same_poly(const edge_type& edge0, const edge_type& edge1, const std::vector<size_t>& segments_to_poly);
};

#endif
