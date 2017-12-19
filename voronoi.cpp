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

multi_ring_fp Voronoi::get_voronoi_rings(
    const multi_polygon_type& input,
    const box_type& mask_bounding_box, coordinate_type max_dist) {
    // Bounding_box is a box that is big enough to hold all milling.
    box_type_fp bounding_box = bg::return_envelope<box_type_fp>(input);
    // Expand that bounding box by the provided bounding_box.
    bg::expand(bounding_box, mask_bounding_box);

    // For each input polygon, add all the segments of all the rings.
    vector<size_t> segments_count;
    segments_count.push_back(0);
    for (const polygon_type& polygon : input) {
        // How many segments in this outer ring?
        size_t current_segments = polygon.outer().size() - 1;
        for (const ring_type& ring : polygon.inners()) {
            current_segments += ring.size() - 1;
        }
        segments_count.push_back(segments_count.back() + current_segments);
    }

    vector<segment_type_p> segments;
    segments.reserve(segments_count.back());
    for (const polygon_type& polygon : input) {
        copy_ring(polygon.outer(), segments);
        for (const ring_type& ring : polygon.inners()) {
            copy_ring(ring, segments);
        }
    }
}
    

multi_linestring_type_fp Voronoi::get_voronoi_edges(
    const multi_polygon_type& input,
    const box_type& mask_bounding_box, coordinate_type max_dist) {
    // Bounding_box is a box that is big enough to hold all milling.
    box_type_fp bounding_box = bg::return_envelope<box_type_fp>(input);
    // Expand that bounding box by the provided bounding_box.
    bg::expand(bounding_box, mask_bounding_box);

    // For each input polygon, add all the segments of all the rings.
    vector<size_t> segments_count;
    segments_count.push_back(0);
    for (const polygon_type& polygon : input) {
        // How many segments in this outer ring?
        size_t current_segments = polygon.outer().size() - 1;
        for (const ring_type& ring : polygon.inners()) {
            current_segments += ring.size() - 1;
        }
        segments_count.push_back(segments_count.back() + current_segments);
    }

    vector<segment_type_p> segments;
    segments.reserve(segments_count.back());
    for (const polygon_type& polygon : input) {
        copy_ring(polygon.outer(), segments);
        for (const ring_type& ring : polygon.inners()) {
            copy_ring(ring, segments);
        }
    }

    voronoi_builder_type voronoi_builder;
    boost::polygon::insert(segments.begin(), segments.end(), &voronoi_builder);
    voronoi_diagram_type voronoi_diagram;
    voronoi_builder.construct(&voronoi_diagram);

    // Get all segments.
    multi_linestring_type_fp output{};
    for (const edge_type& edge : voronoi_diagram.edges()) {
        auto source_index0 = edge.cell()->source_index();
        auto source_index1 = edge.twin()->cell()->source_index();
        size_t segment_index0 = *std::upper_bound(segments_count.cbegin(), segments_count.cend(), source_index0);
        size_t segment_index1 = *std::upper_bound(segments_count.cbegin(), segments_count.cend(), source_index1);
        // Do these two source segments come from different rings?
        if (segment_index0 < segment_index1 && // Only use one side of the half-edges.
            edge.is_primary()) { // Only actual voronoi edges.
            // We want to use this edge.
            linestring_type_fp new_voronoi_edge{};
            if (edge.is_finite()) {
                if (edge.is_linear()) {
                    new_voronoi_edge.push_back(point_type_fp(edge.vertex0()->x(),
                                                             edge.vertex0()->y()));
                    new_voronoi_edge.push_back(point_type_fp(edge.vertex1()->x(),
                                                             edge.vertex1()->y()));
                } else {
                    // It's a curve, it needs sampling.
                    vector<point_type_fp_p> sampled_edge;
                    sample_curved_edge(&edge, segments, sampled_edge, max_dist);
                    for (const auto& point : sampled_edge) {
                        new_voronoi_edge.push_back(point_type_fp(point.x(), point.y()));
                    }
                }
            } else {
                // Infinite edge, only make it if it is inside the bounding_box.
                if ((edge.vertex0() == NULL ||
                     bg::covered_by(point_type(edge.vertex0()->x(), edge.vertex0()->y()),
                                    bounding_box)) &&
                    (edge.vertex1() == NULL ||
                     bg::covered_by(point_type(edge.vertex1()->x(), edge.vertex1()->y()),
                                    bounding_box))) {
                    vector<point_type_fp_p> clipped_edge;
                    clip_infinite_edge(
                        edge, segments, &clipped_edge, bounding_box);
                    for (const auto& point : clipped_edge) {
                        new_voronoi_edge.push_back(point_type_fp(point.x(), point.y()));
                    }
                }
            }
            if (new_voronoi_edge.size() > 0) {
              output.push_back(new_voronoi_edge);
            }
        }
    }
    return output;
}

// Make segments from the ring and put them in segments.
void Voronoi::copy_ring(const ring_type& ring, vector<segment_type_p> &segments)
{
  for (auto iter = ring.begin(); iter + 1 != ring.end(); iter++) {
    segments.push_back(segment_type_p(point_type_p(iter->x(), iter->y()),
				      point_type_p((iter + 1)->x(), (iter + 1)->y())));
  }
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

void Voronoi::clip_infinite_edge(
    const edge_type& edge, const vector<segment_type_p>& segments, std::vector<point_type_fp_p>* clipped_edge, box_type_fp& bounding_box) {
    const cell_type& cell1 = *edge.cell();
    const cell_type& cell2 = *edge.twin()->cell();
    point_type_p origin, direction;
    // Infinite edges could not be created by two segment sites.
    if (cell1.contains_point() && cell2.contains_point()) {
        point_type_p p1 = retrieve_point(cell1, segments);
        point_type_p p2 = retrieve_point(cell2, segments);
        origin.x((p1.x() + p2.x()) * 0.5);
        origin.y((p1.y() + p2.y()) * 0.5);
        direction.x(p1.y() - p2.y());
        direction.y(p2.x() - p1.x());
    } else {
        origin = cell1.contains_segment() ?
            retrieve_point(cell2, segments) :
            retrieve_point(cell1, segments);
        segment_type_p segment = cell1.contains_segment() ?
            retrieve_segment(cell1, segments) :
            retrieve_segment(cell2, segments);
        coordinate_type dx = high(segment).x() - low(segment).x();
        coordinate_type dy = high(segment).y() - low(segment).y();
        if ((low(segment) == origin) ^ cell1.contains_point()) {
            direction.x(dy);
            direction.y(-dx);
        } else {
            direction.x(-dy);
            direction.y(dx);
        }
    }
    coordinate_type side = bounding_box.max_corner().x() - bounding_box.min_corner().x();
    coordinate_type koef =
        side / (std::max)(fabs(direction.x()), fabs(direction.y()));
    if (edge.vertex0() == NULL) {
        clipped_edge->push_back(point_type_fp_p(
                                    origin.x() - direction.x() * koef,
                                    origin.y() - direction.y() * koef));
    } else {
        clipped_edge->push_back(
            point_type_fp_p(edge.vertex0()->x(), edge.vertex0()->y()));
    }
    if (edge.vertex1() == NULL) {
        clipped_edge->push_back(point_type_fp_p(
                                    origin.x() + direction.x() * koef,
                                    origin.y() + direction.y() * koef));
    } else {
        clipped_edge->push_back(
            point_type_fp_p(edge.vertex1()->x(), edge.vertex1()->y()));
    }
}
