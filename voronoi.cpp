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

#include "voronoi.hpp"
#include "voronoi_visual_utils.hpp"

#include <list>
#include <map>
#include <algorithm>
using std::list;
using std::map;

void Voronoi::build_voronoi(const multi_polygon_type& input, multi_polygon_type &output,
                            coordinate_type bounding_box_offset, coordinate_type max_dist)
{
    voronoi_diagram_type voronoi_diagram;
    voronoi_builder_type voronoi_builder;
    vector<segment_type_p> segments;
    list<const cell_type *> visited_cells;
    size_t segments_num = 0;
    ring_type bounding_box_ring;

    bg::assign(bounding_box_ring, bg::return_buffer<box_type>(
                                bg::return_envelope<box_type>(input), bounding_box_offset));

    for (const polygon_type& polygon : input)
    {
        segments_num += polygon.outer().size() - 1;
        
        for (const ring_type& ring : polygon.inners())
        {
            segments_num += ring.size() - 1;
        }
    }
    
    segments_num += bounding_box_ring.size() - 1;
    
    segments.reserve(segments_num);
    
    for (const polygon_type& polygon : input)
    {
        copy_ring(polygon.outer(), segments);

        for (const ring_type& ring : polygon.inners())
        {
            copy_ring(ring, segments);
        }
    }
    
    copy_ring(bounding_box_ring, segments);

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
                                auto append_remove_extra = [&] (const point_type& point)
                                {
                                    /* 
                                     * This works, but it seems more a workaround than a proper solution.
                                     * Why are these segments here in the first place? FIXME
                                     */
                                    if (ring.size() >= 2 && bg::equals(point, *(ring.end() - 2)))
                                        ring.pop_back();
                                    else 
                                        ring.push_back(point);
                                };

                                if (bg::covered_by(startpoint, polygon) && bg::covered_by(endpoint, polygon))
                                {
                                    ring.clear();
                                    break;
                                }

                                if (ring.empty() || startpoint.x() != ring.back().x() || startpoint.y() != ring.back().y())
                                    append_remove_extra(startpoint);

                                if (edge->is_linear())
                                    append_remove_extra(endpoint);
                                else
                                {
                                    vector<point_type_fp_p> sampled_edge;

                                    sample_curved_edge(edge, segments, sampled_edge, max_dist);

                                    for (auto iterator = sampled_edge.begin() + 1; iterator != sampled_edge.end(); iterator++)
                                        append_remove_extra(point_type(iterator->x(), iterator->y()));
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

void Voronoi::copy_ring(const ring_type& ring, vector<segment_type_p> &segments)
{
    for (auto iter = ring.begin(); iter + 1 != ring.end(); iter++)
        segments.push_back(segment_type_p(point_type_p(iter->x(), iter->y()),
                                        point_type_p((iter + 1)->x(), (iter + 1)->y())));
}

point_type_p Voronoi::retrieve_point(const cell_type& cell, const vector<segment_type_p> &segments) {
    source_index_type index = cell.source_index();
    source_category_type category = cell.source_category();

    if (category == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT)
        return boost::polygon::low(segments[index]);
    else
       return boost::polygon::high(segments[index]);
}

const segment_type_p& Voronoi::retrieve_segment(const cell_type& cell, const vector<segment_type_p> &segments) {
    source_index_type index = cell.source_index();
    return segments[index];
}

void Voronoi::sample_curved_edge(const edge_type *edge, const vector<segment_type_p> &segments,
                         vector<point_type_fp_p>& sampled_edge, coordinate_type_fp max_dist) {

    point_type_p point = edge->cell()->contains_point() ?
        retrieve_point(*(edge->cell()), segments) :
        retrieve_point(*(edge->twin()->cell()), segments);

    const segment_type_p& segment = edge->cell()->contains_point() ?
        retrieve_segment(*(edge->twin()->cell()), segments) :
        retrieve_segment(*(edge->cell()), segments);

    sampled_edge.push_back(point_type_fp_p(edge->vertex0()->x(), edge->vertex0()->y()));
    sampled_edge.push_back(point_type_fp_p(edge->vertex1()->x(), edge->vertex1()->y()));

    boost::polygon::voronoi_visual_utils<coordinate_type_fp>::discretize(point, segment, max_dist, &sampled_edge);
}

pair<const polygon_type *,ring_type *> Voronoi::find_ring (const multi_polygon_type& input, const cell_type& cell, multi_polygon_type& output)
{
    const source_index_type index = cell.source_index();
    source_index_type cur_index = 0;

    for (auto i_poly = input.cbegin(); i_poly != input.cend(); i_poly++)
    {
        if (cur_index + i_poly->outer().size() - 1 > index)
        {
            return make_pair(&(*i_poly),
                &(output.at(i_poly - input.cbegin()).outer()));
        }
        
        cur_index += i_poly->outer().size() - 1;
        
        for (auto i_inner = i_poly->inners().cbegin(); i_inner != i_poly->inners().cend(); i_inner++)
        {
            if (cur_index + i_inner->size() - 1 > index)            
                return make_pair(&(*i_poly),
                    &(output.at(i_poly - input.cbegin()).inners().at(i_inner - i_poly->inners().cbegin())));

            cur_index += i_inner->size() - 1;
        }
    }
    
    return make_pair((const polygon_type *) NULL, (ring_type *) NULL);
}

