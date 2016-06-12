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

#include <fstream>
#include <boost/format.hpp>

#include <glibmm/miscutils.h>
using Glib::build_filename;

#include "tsp_solver.hpp"
#include "surface_vectorial.hpp"
#include "voronoi_visual_utils.hpp"

using std::max_element;
using std::next;

Surface_vectorial::Surface_vectorial(unsigned int points_per_circle, ivalue_t width,
                                        ivalue_t height,string name, string outputdir) :
    points_per_circle(points_per_circle),
    width_in(width),
    height_in(height),
    name(name),
    outputdir(outputdir)
{

}

void Surface_vectorial::render(shared_ptr<VectorialLayerImporter> importer)
{
    shared_ptr<multi_polygon_type> vectorial_surface_not_simplified;

    vectorial_surface = make_shared<multi_polygon_type>();
    vectorial_surface_not_simplified = importer->render(points_per_circle);
    
    scale = importer->vectorial_scale();
    
    //With a very small loss of precision we can reduce memory usage and processing time
    bg::simplify(*vectorial_surface_not_simplified, *vectorial_surface, scale / 10000);
    bg::envelope(*vectorial_surface, bounding_box);
}

vector<shared_ptr<icoords> > Surface_vectorial::get_toolpath(shared_ptr<RoutingMill> mill,
        bool mirror, bool mirror_absolute)
{
    coordinate_type grow = mill->tool_diameter / 2 * scale;
    vector<shared_ptr<icoords> > toolpath;
    vector<shared_ptr<icoords> > toolpath_optimised;
    multi_polygon_type voronoi;
    shared_ptr<Isolator> isolator = dynamic_pointer_cast<Isolator>(mill);
    const int extra_passes = isolator ? isolator->extra_passes : 0;
    const coordinate_type mirror_axis = mirror_absolute ? 
        bounding_box.min_corner().x() :
        ((bounding_box.min_corner().x() + bounding_box.max_corner().x()) / 2);

    build_voronoi(*vectorial_surface, voronoi, grow * 3, mill->tolerance * scale);

    init_debug_image(name + ".svg");

    srand(1);
    add_debug_image(voronoi, 0.2, false);
    srand(1);

    for (unsigned int i = 0; i < vectorial_surface->size(); i++)
    {
        shared_ptr<vector<polygon_type> > polygons;
    
        polygons = offset_polygon(*vectorial_surface, voronoi, toolpath, grow,
                        points_per_circle, i, extra_passes + 1, scale, mirror, mirror_axis);
        
        add_debug_image(*polygons, 0.6);
    }

    srand(1);
    add_debug_image(*vectorial_surface, 1, true);
    close_debug_image();

    tsp_solver::nearest_neighbour( toolpath, std::make_pair(0, 0), 0.0001 );

    if (mill->optimise)
    {
        for (const shared_ptr<icoords>& ring : toolpath)
        {
            toolpath_optimised.push_back(make_shared<icoords>());
            bg::simplify(*ring, *(toolpath_optimised.back()), mill->tolerance);
        }

        return toolpath_optimised;
    }
    else
        return toolpath;
}

void Surface_vectorial::save_debug_image(string message)
{
/*
    static unsigned int debug_image_index = 0;
    vector<shared_ptr<const multi_polygon_type> > geometries (1);
    
    geometries.front() = vectorial_surface;
    save_debug_image(geometries, (boost::format("outp%1%_%2%.svg") % debug_image_index % message).str());
    ++debug_image_index;
*/
}

void Surface_vectorial::init_debug_image(string filename)
{
    svg = new std::ofstream(build_filename(outputdir, filename));
    mapper = new bg::svg_mapper<point_type_fp>(*svg, width_in * 1000, height_in * 1000);
    scale_geometry = new bg::strategy::transform::scale_transformer<coordinate_type_fp, 2, 2>(1000.0 / scale);
}

void Surface_vectorial::add_debug_image(const multi_polygon_type& geometry, double opacity, bool stroke)
{
    string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:1" : "";

    for (const polygon_type& poly : geometry)
    {
        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;
        polygon_type_fp poly_scaled;

        bg::transform(poly, poly_scaled, *scale_geometry);

        mapper->add(poly_scaled);
        mapper->map(poly_scaled,
            str(boost::format("fill-opacity:%d;fill:rgb(%d,%d,%d);" + stroke_str) %
            opacity % r % g % b));
    }
}

void Surface_vectorial::add_debug_image(const vector<polygon_type>& geometries, double opacity)
{
    const unsigned int r = rand() % 256;
    const unsigned int g = rand() % 256;
    const unsigned int b = rand() % 256;

    for (unsigned int i = geometries.size(); i != 0; i--)
    {
        polygon_type_fp poly_scaled;
        
        bg::transform(geometries[i - 1], poly_scaled, *scale_geometry);

        mapper->add(poly_scaled);
        
        if (i == geometries.size())
        {
            mapper->map(poly_scaled,
                str(boost::format("fill-opacity:%d;fill:rgb(%d,%d,%d);stroke:rgb(0,0,0);stroke-width:1") %
                opacity % r % g % b));
        }
        else
        {
            mapper->map(poly_scaled, "fill:none;stroke:rgb(0,0,0);stroke-width:1");
        }
    }
}

void Surface_vectorial::close_debug_image()
{
    delete scale_geometry;
    delete mapper;
    delete svg;
}

void Surface_vectorial::group_rings(list<ring_type *> rings, vector<pair<ring_type *, vector<ring_type *> > >& grouped_rings)
{
    map<const ring_type *, coordinate_type> areas;
    auto compare_2nd = [&](const ring_type *a, const ring_type *b) { return areas.at(a) < areas.at(b); };

    for (const ring_type *ring : rings)
        areas[ring] = coordinate_type(bg::area(*ring));

    while (!rings.empty())
    {
        grouped_rings.resize(grouped_rings.size() + 1);
        
        auto biggest_ring = max_element(rings.begin(), rings.end(), compare_2nd);
        pair<ring_type *, vector<ring_type *> >& current_ring = grouped_rings.back();
        forward_list<list<ring_type *>::iterator> to_be_removed_rings;

        current_ring.first = *biggest_ring;
        rings.erase(biggest_ring);

        for (auto i = rings.begin(); i != rings.end(); i++)
        {
            if (bg::covered_by(**i, **biggest_ring))
            {
                list<ring_type *>::iterator j;
            
                for (j = rings.begin(); j != rings.end(); j++)
                    if (*i != *j && bg::covered_by(**i, **j))
                            break;

                if (j == rings.end())
                {
                    current_ring.second.push_back(*i);
                    to_be_removed_rings.push_front(i);
                }
            }
        }
        
        for (auto i : to_be_removed_rings)
            rings.erase(i);
    }
}

void Surface_vectorial::fill_outline(double linewidth)
{
    map<const ring_type *, const polygon_type *> rings_map;
    vector<pair<ring_type *, vector<ring_type *> > > grouped_rings;
    list<ring_type *> rings;
    auto filled_outline = make_shared<multi_polygon_type>();

    for (polygon_type& polygon : *vectorial_surface)
    {
        rings.push_back(&(polygon.outer()));
        rings_map[&(polygon.outer())] = &polygon;
    }

    group_rings(rings, grouped_rings);
    filled_outline->resize(grouped_rings.size());

    for (unsigned int i = 0; i < grouped_rings.size(); i++)
    {
        const polygon_type *outer_polygon = rings_map.at(grouped_rings[i].first);

        (*filled_outline)[i].outer() = outer_polygon->outer();

        for (ring_type* ring : grouped_rings[i].second)
        {
            const vector<ring_type>& inners = rings_map.at(ring)->inners();

            if (!inners.empty())
                (*filled_outline)[i].inners().push_back(inners.front());
            else
                (*filled_outline)[i].inners().push_back(rings_map[ring]->outer());
        }
    }

    vectorial_surface = filled_outline;
}

void Surface_vectorial::add_mask(shared_ptr<Core> surface)
{
    shared_ptr<multi_polygon_type> masked_vectorial_surface = make_shared<multi_polygon_type>();
    shared_ptr<Surface_vectorial> mask = dynamic_pointer_cast<Surface_vectorial>(surface);
    
    if (mask)
    {
        bg::intersection(*vectorial_surface, *(mask->vectorial_surface), *masked_vectorial_surface);
        swap(masked_vectorial_surface, vectorial_surface);
    }
    else
        throw std::logic_error("Can't cast Core to Surface_vectorial");
}

point_type_p Surface_vectorial::retrieve_point(const cell_type& cell, const vector<segment_type_p> &segments) {
    source_index_type index = cell.source_index();
    source_category_type category = cell.source_category();

    if (category == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT)
        return boost::polygon::low(segments[index]);
    else
       return boost::polygon::high(segments[index]);
}

segment_type_p Surface_vectorial::retrieve_segment(const cell_type& cell, const vector<segment_type_p> &segments) {
    source_index_type index = cell.source_index();
    return segments[index];
}

void Surface_vectorial::sample_curved_edge(const edge_type *edge, const vector<segment_type_p> &segments,
                         vector<point_type_fp_p>& sampled_edge, coordinate_type_fp max_dist) {

    point_type_p point = edge->cell()->contains_point() ?
        retrieve_point(*(edge->cell()), segments) :
        retrieve_point(*(edge->twin()->cell()), segments);
    
    segment_type_p segment = edge->cell()->contains_point() ?
        retrieve_segment(*(edge->twin()->cell()), segments) :
        retrieve_segment(*(edge->cell()), segments);

    sampled_edge.push_back(point_type_fp_p(edge->vertex0()->x(), edge->vertex0()->y()));
    sampled_edge.push_back(point_type_fp_p(edge->vertex1()->x(), edge->vertex1()->y()));

    boost::polygon::voronoi_visual_utils<coordinate_type_fp>::discretize(point, segment, max_dist, &sampled_edge);
}

void Surface_vectorial::copy_ring(const ring_type& ring, vector<segment_type_p> &segments)
{
    for (auto iter = ring.begin(); iter + 1 != ring.end(); iter++)
        segments.push_back(segment_type_p(point_type_p(iter->x(), iter->y()),
                                        point_type_p((iter + 1)->x(), (iter + 1)->y())));
}

pair<const polygon_type *,ring_type *> Surface_vectorial::find_ring (const multi_polygon_type& input, const cell_type& cell, multi_polygon_type& output)
{
    const source_index_type index = cell.source_index();
    source_index_type cur_index = 0;

    for (auto i_poly = input.cbegin(); i_poly != input.cend(); i_poly++)
    {
        if (cur_index + i_poly->outer().size() - 1 > index)
        {
            return std::make_pair(&(*i_poly),
                &(output.at(i_poly - input.cbegin()).outer()));
        }
        
        cur_index += i_poly->outer().size() - 1;
        
        for (auto i_inner = i_poly->inners().cbegin(); i_inner != i_poly->inners().cend(); i_inner++)
        {
            if (cur_index + i_inner->size() - 1 > index)            
                return std::make_pair(&(*i_poly),
                    &(output.at(i_poly - input.cbegin()).inners().at(i_inner - i_poly->inners().cbegin())));

            cur_index += i_inner->size() - 1;
        }
    }
    
    return std::make_pair((const polygon_type *) NULL, (ring_type *) NULL);
}

void Surface_vectorial::append_remove_extra(ring_type& ring, const point_type point)
{
    /* 
     * This works, but it seems more a workaround than a proper solution.
     * Why are these segments here in the first place? FIXME
     */
    if (ring.size() >= 2 && bg::equals(point, *(ring.end() - 2)))
        ring.pop_back();
    else 
        ring.push_back(point);
}

void Surface_vectorial::build_voronoi(const multi_polygon_type& input, multi_polygon_type &output, coordinate_type bounding_box_offset, coordinate_type max_dist)
{
    voronoi_diagram_type voronoi_diagram;
    voronoi_builder_type voronoi_builder;
    vector<segment_type_p> segments;
    std::list<const cell_type *> visited_cells;
    size_t segments_num = 0;
    ring_type bounding_box;
    
    bg::assign(bounding_box, bg::return_buffer<box_type>(
                                    bg::return_envelope<box_type>(input), bounding_box_offset));

    for (const polygon_type& polygon : input)
    {
        segments_num += polygon.outer().size() - 1;
        
        for (const ring_type& ring : polygon.inners())
        {
            segments_num += ring.size() - 1;
        }
    }
    
    segments_num += bounding_box.size() - 1;
    
    segments.reserve(segments_num);
    
    for (const polygon_type& polygon : input)
    {
        copy_ring(polygon.outer(), segments);

        for (const ring_type& ring : polygon.inners())
        {
            copy_ring(ring, segments);
        }
    }
    
    copy_ring(bounding_box, segments);

    output.resize(input.size());
    for (size_t i = 0; i < input.size(); i++)
        output.at(i).inners().resize(input.at(i).inners().size());

    boost::polygon::insert(segments.begin(), segments.end(), &voronoi_builder);
    voronoi_builder.construct(&voronoi_diagram);

    for (const cell_type& cell : voronoi_diagram.cells())
    {
        if (!cell.is_degenerate())
        {
            const edge_type *edge = cell.incident_edge();
            const cell_type *last_cell = NULL;
            const auto found_cell = std::find(visited_cells.begin(), visited_cells.end(), &cell);

            bool backwards = false;

            if (found_cell == visited_cells.end())
            {
                pair<const polygon_type *, ring_type *> related_geometries = find_ring(input, cell, output);
                
                if (related_geometries.first && related_geometries.second)
                {
                    const polygon_type& polygon = *(related_geometries.first);
                    ring_type& ring = *(related_geometries.second);

                    do {
                        if (edge->is_primary())
                        {
                            if (edge->is_finite())
                            {
                                const point_type startpoint (edge->vertex0()->x(), edge->vertex0()->y());
                                const point_type endpoint (edge->vertex1()->x(), edge->vertex1()->y());

                                if (bg::within(startpoint, polygon) && bg::within(endpoint, polygon))
                                {
                                    ring.clear();
                                    break;
                                }

                                if (ring.empty() || startpoint.x() != ring.back().x() || startpoint.y() != ring.back().y())
                                    append_remove_extra(ring, startpoint);

                                if (edge->is_linear())
                                    append_remove_extra(ring, endpoint);
                                else
                                {
                                    vector<point_type_fp_p> sampled_edge;

                                    sample_curved_edge(edge, segments, sampled_edge, max_dist);

                                    for (auto iterator = sampled_edge.begin() + 1; iterator != sampled_edge.end(); iterator++)
                                        append_remove_extra(ring, point_type(iterator->x(), iterator->y()));
                                }

                            }
                            else
                            {
                                ring.clear();
                                break;
                            }
                        }
                        else
                        {
                            if (!backwards)
                            {
                                const cell_type *current_cell = edge->cell();
                                const cell_type *next_cell = edge->twin()->cell();

                                if (next_cell == last_cell)
                                    backwards = true;
                                else
                                    if (current_cell != &cell)
                                        visited_cells.push_front(current_cell);

                                last_cell = current_cell;
                            }
                            
                            edge = edge->twin();
                        }

                        edge = edge->next();
                    } while (edge != cell.incident_edge());
                }
            }
            else
                visited_cells.erase(found_cell);
        }
    }
    
    bg::correct(output);
}

shared_ptr<vector<polygon_type> > Surface_vectorial::offset_polygon(const multi_polygon_type& input,
                            const multi_polygon_type& voronoi, vector< shared_ptr<icoords> >& toolpath,
                            coordinate_type offset, unsigned int points_per_circle, size_t index,
                            unsigned int steps, coordinate_type scale, bool mirror, ivalue_t mirror_axis)
{
    auto polygons = make_shared<vector<polygon_type> >(steps);
    list<list<const ring_type *> > rings (steps);
    auto ring_i = rings.begin();

    auto push_point = [&](const point_type& point)
    {
        if (mirror)
            toolpath.back()->push_back(make_pair((2 * mirror_axis - point.x()) / double(scale),
                                                    point.y() / double(scale)));
        else
            toolpath.back()->push_back(make_pair(point.x() / double(scale),
                                                    point.y() / double(scale)));
    };

    auto copy_ring_to_toolpath = [&](const ring_type& ring, unsigned int start)
    {
        const auto size_minus_1 = ring.size() - 1;
        unsigned int i = start;

        do
        {
            push_point(ring[i]);
            i = (i + 1) % size_minus_1;
        } while (i != start);

        push_point(ring[i]);
    };

    auto find_first_nonempty = [&]()
    {
        for (auto i = rings.begin(); i != rings.end(); i++)
            if (!i->empty())
                return i;
        return rings.end();
    };

    auto find_closest_point_index = [&](const ring_type& ring)
    {
        const unsigned int size = ring.size();
        const icoordpair& last_icp = toolpath.back()->back();
        point_type last_point;

        last_point.y(last_icp.second * scale);
        if (mirror)
            last_point.x(2 * mirror_axis - last_icp.first * scale);
        else
            last_point.x(last_icp.first * scale);

        auto min_distance = bg::comparable_distance(ring[0], last_point);
        unsigned int index = 0;

        for (unsigned int i = 1; i < size; i++)
        {
            const auto distance = bg::comparable_distance(ring[i], last_point);

            if (distance < min_distance)
            {
                min_distance = distance;
                index = i;
            }
        }

        return index;
    };

    toolpath.push_back(make_shared<icoords>());

    for (unsigned int i = 0; i < steps; i++)
    {
        multi_polygon_type mpoly_temp;
        multi_polygon_type mpoly;

        bg::buffer(input[index], mpoly_temp,
                   bg::strategy::buffer::distance_symmetric<coordinate_type>(offset * (i + 1)),
                   bg::strategy::buffer::side_straight(),
                   bg::strategy::buffer::join_miter(5 * offset * (i + 1)),
                   bg::strategy::buffer::end_round(points_per_circle),
                   bg::strategy::buffer::point_circle(points_per_circle));

        bg::intersection(mpoly_temp, voronoi[index], mpoly);

        (*polygons)[i] = mpoly[0];
        mpoly.clear();

        if (i == 0)
            copy_ring_to_toolpath((*polygons)[i].outer(), 0);
        else
            copy_ring_to_toolpath((*polygons)[i].outer(), find_closest_point_index((*polygons)[i].outer()));

        for (const ring_type& ring : (*polygons)[i].inners())
            ring_i->push_back(&ring);
        
        ++ring_i;
    }

    ring_i = find_first_nonempty();

    while (ring_i != rings.end())
    {
        const ring_type *biggest = ring_i->front();
        auto ring_j = next(ring_i);

        toolpath.push_back(make_shared<icoords>());
        copy_ring_to_toolpath(*biggest, 0);

        while (ring_j != rings.end())
        {
            list<const ring_type *>::iterator j;

            for (j = ring_j->begin(); j != ring_j->end(); j++)
                if (bg::covered_by(**j, *biggest))
                {
                    copy_ring_to_toolpath(**j, find_closest_point_index(**j));
                    ring_j->erase(j);
                    break;
                }

            if (j == ring_j->end())
                ring_j = rings.end();
            else
                ++ring_j;
        }

        ring_i->erase(ring_i->begin());
        ring_i = find_first_nonempty();
    }

    return polygons;
}

