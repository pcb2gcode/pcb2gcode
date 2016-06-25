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
            bool mirror, bool mirror_absolute);
    void save_debug_image(string message);
    void init_debug_image(string filename);
    void add_debug_image(const multi_polygon_type& geometry, double opacity, bool stroke);
    void add_debug_image(const vector<polygon_type>& geometries, double opacity);
    void close_debug_image();
    void fill_outline(double linewidth);
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

    shared_ptr<multi_polygon_type> vectorial_surface;
    coordinate_type scale;
    box_type bounding_box;
    
    shared_ptr<Surface_vectorial> mask;

    std::ofstream *svg;
    bg::svg_mapper<point_type_fp> *mapper;
    bg::strategy::transform::scale_transformer<coordinate_type_fp, 2, 2> *scale_geometry;

    shared_ptr<vector<polygon_type> > offset_polygon(const multi_polygon_type& input,
                            const multi_polygon_type& voronoi, vector< shared_ptr<icoords> >& toolpath,
                            coordinate_type offset, size_t index, unsigned int steps,
                            bool mirror, ivalue_t mirror_axis);

    static void group_rings(list<ring_type *> rings, vector<pair<ring_type *, vector<ring_type *> > >& grouped_rings);
};

#endif // SURFACE_VECTORIAL_H
