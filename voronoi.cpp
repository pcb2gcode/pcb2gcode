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

 #include <iostream>
 using std::cerr;
 using std::cout;
 using std::endl;

#include "voronoi.hpp"
#include "voronoi_visual_utils.hpp"
#include <list>
#include <map>
#include <algorithm>
using std::list;
using std::map;

#include <vector>
using std::vector;

// For use when we have to convert from float to long and back.
const double SCALE = 1000000.0;

multi_polygon_type_fp Voronoi::build_voronoi(
    const multi_polygon_type_fp& input,
    const box_type_fp& mask_bounding_box, coordinate_type_fp max_dist) {
  // We need to scale all the inputs and call the integer version.
  multi_polygon_type_fp scaled_input;
  bg::transform(input, scaled_input,
                bg::strategy::transform::scale_transformer<coordinate_type_fp, 2, 2>(
                    SCALE, SCALE));

  box_type_fp scaled_mask_bounding_box;
  bg::transform(mask_bounding_box, scaled_mask_bounding_box,
                bg::strategy::transform::scale_transformer<coordinate_type_fp, 2, 2>(
                    SCALE, SCALE));

  multi_polygon_type voronoi_input;
  bg::convert(scaled_input, voronoi_input);
  box_type voronoi_bounding_box;
  bg::convert(scaled_mask_bounding_box, voronoi_bounding_box);

  const multi_polygon_type_fp scaled_voronoi = build_voronoi(voronoi_input, voronoi_bounding_box, max_dist * SCALE);
  // Scale the result back down.
  multi_polygon_type_fp voronoi;
  bg::transform(scaled_voronoi, voronoi,
                bg::strategy::transform::scale_transformer<coordinate_type_fp, 2, 2>(
                    1.0/SCALE, 1.0/SCALE));
  return voronoi;
}

multi_polygon_type_fp Voronoi::build_voronoi(
    const multi_polygon_type& input,
    const box_type& mask_bounding_box, coordinate_type max_dist) {
    if (input.empty()) {
        return multi_polygon_type_fp();
    }

    // Bounding_box is a box that is big enough to hold all milling.
    box_type_fp bounding_box = bg::return_envelope<box_type_fp>(input);
    // Expand that bounding box by the provided bounding_box.
    bg::expand(bounding_box, mask_bounding_box);
    // Make it large enough so that any voronoi edges between it and
    // the input will surely line outside mask_bounding_box.
    const auto bounding_box_width = bounding_box.max_corner().x() - bounding_box.min_corner().x();
    const auto bounding_box_height = bounding_box.max_corner().y() - bounding_box.min_corner().y();
    bounding_box.max_corner().x(bounding_box.max_corner().x() + 2*bounding_box_width);
    bounding_box.min_corner().x(bounding_box.min_corner().x() - 2*bounding_box_width);
    bounding_box.max_corner().y(bounding_box.max_corner().y() + 2*bounding_box_height);
    bounding_box.min_corner().y(bounding_box.min_corner().y() - 2*bounding_box_height);
    // Each line segment from all the inputs.
    vector<segment_type_p> segments;
    // From which polygon each segment is sourced.
    std::vector<size_t> segments_to_poly;
    for (const auto& polygon : input) {
        copy_ring(polygon.outer(), segments);
        for (const ring_type& ring : polygon.inners()) {
            copy_ring(ring, segments);
        }
        segments_to_poly.push_back(segments.size());
    }

    // Add the bounding box but without putting it in segments_to_poly
    // so that we won't put voronoi cells from it into the output.
    ring_type bounding_box_ring;
    bg::convert(bounding_box, bounding_box_ring);
    copy_ring(bounding_box_ring, segments);

    voronoi_builder_type voronoi_builder;
    boost::polygon::insert(segments.begin(), segments.end(), &voronoi_builder);
    voronoi_diagram_type voronoi_diagram;
    voronoi_builder.construct(&voronoi_diagram);

    // The output polygons which are voronoi shapes.  The outputs
    // match the inputs in number and position but the number of inner
    // rings for each output polygon might not match.
    multi_polygon_type_fp output;
    // Pre-allocate because we are going to add elements out of order.
    output.resize(input.size());

    // To keep it simply, we first mark each edge as used if it won't
    // be part of the output.
    const int VISITED = 1;
    for (const edge_type& edge : voronoi_diagram.edges()) {
        if (!edge.is_primary() || // Not a real voronoi edge.
            same_poly(edge, *edge.twin(), segments_to_poly) || // This edge doesn't divide different shapes.
            // The edge belongs to the boundary.
            std::upper_bound(segments_to_poly.cbegin(), segments_to_poly.cend(), edge.twin()->cell()->source_index()) == segments_to_poly.cend()) {
            edge.color(edge.color() | VISITED);
        }
    }
    for (const edge_type& edge : voronoi_diagram.edges()) {
        if ((edge.color() & VISITED) == VISITED) {
            continue;  // Used already.
        }

        const edge_type* current_edge = &edge;
        const edge_type* const start_edge = current_edge;
        ring_type_fp ring;
        do {
            linestring_type_fp discrete_edge = edge_to_linestring(*current_edge, segments, bounding_box, max_dist);
            // Don't push the last one because we'll put it at the end.
            for(std::size_t i=0; i<discrete_edge.size()-1; ++i)
            {
              bg::append(ring, discrete_edge.at(i));
            }

            current_edge->color(current_edge->color() | 1);  // Mark used
            current_edge = current_edge->next();

            // Check that we are still circling the same polygon for
            // our ring, and that we are on edge that is between
            // different traces, and that it's a primary edge.
            while (current_edge != start_edge &&
                   ((current_edge->color() & VISITED) == VISITED ||
                    // Still circling the same twin.  We look at twin because we need clockwise outer loops.
                    !same_poly(*current_edge->twin(), *start_edge->twin(), segments_to_poly))) {
                current_edge = current_edge->rot_next();
            }
        } while (current_edge != start_edge);

        if(!ring.empty())
        {
          ring.push_back(ring.front());  // Close the ring.
          size_t poly_index = std::distance(segments_to_poly.cbegin(), std::upper_bound(segments_to_poly.cbegin(), segments_to_poly.cend(), edge.twin()->cell()->source_index()));
          if (bg::area(ring) > 0) {
              // This is the outer ring of the poly.
              output[poly_index].outer().swap(ring);
          } else {
              // This has negative area, it must be a hole in the outer.
              output[poly_index].inners().push_back(ring);
          }
        }
    }

    return output;
}

bool Voronoi::same_poly(const edge_type& edge0, const edge_type& edge1, const std::vector<size_t>& segments_to_poly) {
    return (std::upper_bound(segments_to_poly.cbegin(), segments_to_poly.cend(), edge0.cell()->source_index()) ==
            std::upper_bound(segments_to_poly.cbegin(), segments_to_poly.cend(), edge1.cell()->source_index()));
}

linestring_type_fp Voronoi::edge_to_linestring(const edge_type& edge, const vector<segment_type_p>& segments, const box_type_fp& bounding_box, coordinate_type max_dist) {
    linestring_type_fp new_voronoi_edge;
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
    return new_voronoi_edge;
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
    const edge_type& edge, const vector<segment_type_p>& segments, std::vector<point_type_fp_p>* clipped_edge, const box_type_fp& bounding_box) {
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
        side / std::max(std::abs(direction.x()), std::abs(direction.y()));
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
