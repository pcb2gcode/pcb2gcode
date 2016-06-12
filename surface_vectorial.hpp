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

namespace bg = boost::geometry;

/******************************************************************************/
/*
 */
/******************************************************************************/
class Surface_vectorial: public Core, virtual public boost::noncopyable
{
public:
    Surface_vectorial(unsigned int points_per_circle, string name, string outputdir);

    vector<shared_ptr<icoords> > get_toolpath(shared_ptr<RoutingMill> mill,
            bool mirror, bool mirror_absolute);
    void save_debug_image(string message);
    void init_debug_image(string filename, const multi_polygon_type& geometry, double opacity, bool stroke);
    void add_debug_image(const multi_polygon_type& geometry, double opacity, bool stroke);
    void add_debug_image(const vector<polygon_type>& geometries, double opacity);
    void close_debug_image();
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
    const string name;
    const string outputdir;

    shared_ptr<multi_polygon_type> vectorial_surface;
    coordinate_type scale;
    box_type bounding_box;
    
    std::ofstream *svg;
    bg::svg_mapper<point_type_fp> *mapper;
    bg::strategy::transform::scale_transformer<coordinate_type_fp, 2, 2> *scale_geometry;
    bg::strategy::transform::translate_transformer<coordinate_type_fp, 2, 2> *translate_geometry;

    static point_type_p retrieve_point(const cell_type& cell, const vector<segment_type_p> &segments);
    static segment_type_p retrieve_segment(const cell_type& cell, const vector<segment_type_p> &segments);
    static void sample_curved_edge(const edge_type *edge, const vector<segment_type_p> &segments, vector<point_type_fp_p>& sampled_edge, coordinate_type_fp max_dist);
    static void copy_ring(const ring_type& ring, vector<segment_type_p> &segments);

    static pair<const polygon_type *,ring_type *> find_ring (const multi_polygon_type& input,
                                                             const cell_type& cell, multi_polygon_type& output);

    static void append_remove_extra(ring_type& ring, const point_type point);

    static void build_voronoi(const multi_polygon_type& input, multi_polygon_type &output,
                                coordinate_type bounding_box_offset, coordinate_type max_dist);

    static shared_ptr<vector<polygon_type> > offset_polygon(const multi_polygon_type& input,
                            const multi_polygon_type& voronoi, vector< shared_ptr<icoords> >& toolpath,
                            coordinate_type offset, unsigned int points_per_circle, size_t index,
                            unsigned int steps, coordinate_type scale, bool mirror, ivalue_t mirror_axis);

    static void group_rings(list<ring_type *> rings, vector<pair<ring_type *, vector<ring_type *> > >& grouped_rings);
};

#endif // SURFACE_VECTORIAL_H
