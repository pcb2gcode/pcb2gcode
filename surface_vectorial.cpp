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

#include <unordered_set>
using std::unordered_set;

#include <unordered_map>
using std::unordered_map;

#include <fstream>
#include <limits>
using std::numeric_limits;

#include <string>
using std::string;

#include <memory>
using std::make_shared;
using std::shared_ptr;
using std::unique_ptr;

#include <vector>
using std::vector;

#include <algorithm>
#include <iostream>
#include <cmath>
using std::cerr;
using std::endl;

#include <utility>
using std::pair;
using std::make_pair;

#include <tuple>
using std::tuple;
using std::get;

#include <map>
using std::map;

#include <boost/format.hpp>
#include <boost/optional.hpp>
using boost::optional;
using boost::make_optional;

#include "flatten.hpp"
#include "tsp_solver.hpp"
#include "surface_vectorial.hpp"
#include "segmentize.hpp"
#include "eulerian_paths.hpp"
#include "backtrack.hpp"
#include "bg_operators.hpp"
#include "bg_helpers.hpp"
#include "units.hpp"
#include "path_finding.hpp"
#include "trim_paths.hpp"
#include "svg_writer.hpp"
#include "disjoint_set.hpp"
#include "consistent_rand.hpp"

using std::max;
using std::max_element;
using std::next;
using std::dynamic_pointer_cast;

unsigned int Surface_vectorial::debug_image_index = 0;

Surface_vectorial::Surface_vectorial(const box_type_fp& bounding_box,
                                     string name, string outputdir,
                                     bool tsp_2opt, MillFeedDirection::MillFeedDirection mill_feed_direction,
                                     bool invert_gerbers, bool render_paths_to_shapes) :
    bounding_box(bounding_box),
    name(name),
    outputdir(outputdir),
    tsp_2opt(tsp_2opt),
    fill(false),
    mill_feed_direction(mill_feed_direction),
    invert_gerbers(invert_gerbers),
    render_paths_to_shapes(render_paths_to_shapes) {}

void Surface_vectorial::render(shared_ptr<GerberImporter> importer, double tolerance) {
  auto vectorial_surface_not_simplified = importer->render(fill, render_paths_to_shapes);

  if (bg::intersects(vectorial_surface_not_simplified.first)) {
    cerr << "\nWarning: Geometry of layer '" << name << "' is"
        " self-intersecting. This can cause pcb2gcode to produce"
        " wildly incorrect toolpaths. You may want to check the"
        " g-code output and/or fix your gerber files!\n";
  }

  vectorial_surface = make_shared<
      pair<multi_polygon_type_fp, map<coordinate_type_fp, multi_linestring_type_fp>>>();
  if (tolerance > 0) {
    //With a very small loss of precision we can reduce memory usage and processing time
    bg::simplify(vectorial_surface_not_simplified.first, vectorial_surface->first, tolerance);
  } else {
    vectorial_surface->first.swap(vectorial_surface_not_simplified.first);
  }
  for (auto& diameter_and_path : vectorial_surface_not_simplified.second) {
    vectorial_surface->second[diameter_and_path.first] = multi_linestring_type_fp();
    if (tolerance > 0) {
      bg::simplify(diameter_and_path.second,
                   vectorial_surface->second[diameter_and_path.first],
                   tolerance);
    } else {
      vectorial_surface->second[diameter_and_path.first].swap(diameter_and_path.second);
    }
  }
}

// If the direction is ccw, return cw and vice versa.  If any, return any.
MillFeedDirection::MillFeedDirection invert(const MillFeedDirection::MillFeedDirection& dir) {
  if (dir == MillFeedDirection::CLIMB) {
    return MillFeedDirection::CONVENTIONAL;
  } else if (dir == MillFeedDirection::CONVENTIONAL) {
    return MillFeedDirection::CLIMB;
  } else {
    return dir;
  }
}

multi_linestring_type_fp mirror_toolpath(const multi_linestring_type_fp& mls, bool mirror, bool ymirror) {
  multi_linestring_type_fp result;
  for (const auto& ls : mls) {
    linestring_type_fp new_ls;
    for (const auto& point : ls) {
      new_ls.push_back(
        point_type_fp(
          (mirror && !ymirror ? -point.x() : point.x()),
          (mirror &&  ymirror ? -point.y() : point.y())
        )
      );
    }
    result.push_back(new_ls);
  }
  return result;
}

// Find all potential thermal reliefs.  Those are usually holes in traces.
// Return those shapes as rings with correct orientation.
vector<polygon_type_fp> find_thermal_reliefs(const multi_polygon_type_fp& milling_surface,
                                             const coordinate_type_fp tolerance) {
  // For each shape, see if it has any holes that are empty.
  vector<polygon_type_fp> holes;
  for (const auto& p : milling_surface) {
    for (const auto& inner : p.inners()) {
      auto thermal_hole = inner;
      bg::correct(thermal_hole); // Convert it from a hole to a filled-in shape.
      multi_polygon_type_fp shrunk_thermal_hole = 
          bg_helpers::buffer_miter(thermal_hole, -tolerance);
      bool empty_hole = !bg::intersects(shrunk_thermal_hole, milling_surface);
      if (!empty_hole) {
        continue;
      }
      polygon_type_fp p;
      p.outer() = thermal_hole;
      holes.push_back(p);
    }
  }
  return holes;
}

void Surface_vectorial::write_svgs(const std::string& tool_suffix, coordinate_type_fp tool_diameter,
                const multi_linestring_type_fp& toolpaths,
                coordinate_type_fp tolerance, bool find_contentions) const {
  vector<vector<pair<linestring_type_fp, bool>>> new_trace_toolpaths;
  new_trace_toolpaths.emplace({});
  for (const auto& ls : toolpaths) {
    new_trace_toolpaths.front().push_back(make_pair(ls, true));
  }
  write_svgs(tool_suffix, tool_diameter, new_trace_toolpaths, tolerance, find_contentions);
}

void Surface_vectorial::write_svgs(const string& tool_suffix, coordinate_type_fp tool_diameter,
                                   const vector<vector<pair<linestring_type_fp, bool>>>& new_trace_toolpaths,
                                   coordinate_type_fp tolerance, bool find_contentions) const {
  // Now set up the debug images, one per tool.
  svg_writer debug_image(build_filename(outputdir, "processed_" + name + tool_suffix + ".svg"), bounding_box);
  svg_writer traced_debug_image(build_filename(outputdir, "traced_" + name + tool_suffix + ".svg"), bounding_box);
  optional<svg_writer> contentions_image;
  ConsistentRand::srand(1);
  debug_image.add(voronoi, 0.2, false);
  ConsistentRand::srand(1);
  const auto trace_count = new_trace_toolpaths.size();
  for (size_t trace_index = 0; trace_index < trace_count; trace_index++) {
    const auto& new_trace_toolpath = new_trace_toolpaths[trace_index];
    const unsigned int r = ConsistentRand::rand() % 256;
    const unsigned int g = ConsistentRand::rand() % 256;
    const unsigned int b = ConsistentRand::rand() % 256;
    for (const auto& ls_and_allow_reversal : new_trace_toolpath) {
      debug_image.add(ls_and_allow_reversal.first, tool_diameter, r, g, b);
      traced_debug_image.add(ls_and_allow_reversal.first, tool_diameter, r, g, b);
    }

    if (find_contentions) {
      if (trace_index < vectorial_surface->first.size()) {
        multi_polygon_type_fp temp =
            bg_helpers::buffer(vectorial_surface->first.at(trace_index), tool_diameter/2 - tolerance);
        multi_linestring_type_fp temp2;
        for (const auto& ls_and_allow_reversal : new_trace_toolpath) {
          temp2.push_back(ls_and_allow_reversal.first);
        }
        temp2 = temp2 & temp;
        if (bg::length(temp2) > 0) {
          if (!contentions_image) {
            contentions_image.emplace(build_filename(outputdir, "contentions_" + name + tool_suffix + ".svg"), bounding_box);
          }
          contentions_image->add(temp2, tool_diameter, 255, 0, 0);
        }
      }
    }
  }
  if (contentions_image) {
    cerr << "\nWarning: pcb2gcode hasn't been able to fulfill all"
        " clearance requirements.  Check the contentions output"
        " and consider using a smaller milling bit.\n";
  }
  ConsistentRand::srand(1);
  debug_image.add(vectorial_surface->first, 1, true);
  for (const auto& diameter_and_path : vectorial_surface->second) {
    debug_image.add(diameter_and_path.second, diameter_and_path.first, true);
  }
}

vector<pair<linestring_type_fp, bool>> full_eulerian_paths(
    const std::shared_ptr<RoutingMill>& mill,
    const vector<pair<linestring_type_fp, bool>>& toolpath) {
  auto toolpath1 = toolpath;
  toolpath1 = segmentize::segmentize_paths(toolpath1);
  toolpath1 = segmentize::unique(toolpath1);

  vector<pair<linestring_type_fp, bool>> paths_to_add;
  paths_to_add = backtrack::backtrack(
      toolpath1,
      mill->feed,
      (mill->zsafe - mill->zwork) / mill->g0_vertical_speed,
      mill->g0_vertical_speed,
      (mill->zsafe - mill->zwork) / mill->vertfeed,
      mill->backtrack);
  for (const auto& p : paths_to_add) {
    toolpath1.push_back(p);
  }
  toolpath1 = eulerian_paths::get_eulerian_paths<
    point_type_fp,
    linestring_type_fp>(toolpath1);
  trim_paths::trim_paths(toolpath1, paths_to_add);
  return toolpath1;
}

// Make eulerian paths if needed.  Sort the paths order to make it faster.
// Simplify paths by removing points that don't affect the path or affect it
// very little.
multi_linestring_type_fp Surface_vectorial::post_process_toolpath(
    const std::shared_ptr<RoutingMill>& mill,
    const boost::optional<const path_finding::PathFindingSurface*>& path_finding_surface,
    vector<pair<linestring_type_fp, bool>> toolpath1) const {
  if (mill->eulerian_paths) {
    toolpath1 = full_eulerian_paths(mill, toolpath1);
  }
  if (path_finding_surface) {
    const auto extra_paths = final_path_finder(mill, **path_finding_surface, toolpath1);
    if (extra_paths.size() > 0) {
      toolpath1.insert(toolpath1.cend(), extra_paths.cbegin(), extra_paths.cend());
      if (mill->eulerian_paths) {
        toolpath1 = full_eulerian_paths(mill, toolpath1);
      }
    }
  }
  multi_linestring_type_fp combined_toolpath;
  combined_toolpath.reserve(toolpath1.size());
  for (const auto& ls_and_allow_reversal : toolpath1) {
    combined_toolpath.push_back(ls_and_allow_reversal.first);
  }
  shared_ptr<Isolator> isolator = dynamic_pointer_cast<Isolator>(mill);
  if (isolator != nullptr) {
    if (tsp_2opt) {
      tsp_solver::tsp_2opt(combined_toolpath, point_type_fp(0, 0));
    } else {
      tsp_solver::nearest_neighbour(combined_toolpath, point_type_fp(0, 0));
    }
  } else {
    // It's a cutter so do the cuts from shortest to longest.  This
    // makes it very likely that the inside cuts will happen before
    // the perimeter cut, which is best for stability of the PCB.
    std::sort(combined_toolpath.begin(), combined_toolpath.end(),
              [](const linestring_type_fp& lhs, const linestring_type_fp rhs) {
                return bg::length(lhs) < bg::length(rhs);
              });
  }

  if (mill->optimise) {
    multi_linestring_type_fp temp_mls;
    bg::simplify(combined_toolpath, temp_mls, mill->optimise);
    combined_toolpath = temp_mls;
  }
  return combined_toolpath;
}

// Given a linestring which has the same front and back (so it's actually a
// ring), attach it to one of the ends of the toolpath.  Only attach if there is
// a point on the ring that is close enough to the toolpath endpoint.  toolpath
// must not be empty.
bool attach_ring(const linestring_type_fp& ring,
                 pair<linestring_type_fp, bool>& toolpath_and_allow_reversal, // true if the toolpath can be reversed
                 const MillFeedDirection::MillFeedDirection& dir,
                 const Surface_vectorial::PathFinder& path_finder) {
  auto& toolpath = toolpath_and_allow_reversal.first;
  bool insert_at_front = true;
  auto best_ring_point = ring.cbegin();
  double best_distance = bg::comparable_distance(*best_ring_point, toolpath.front());
  for (auto ring_point = ring.cbegin(); ring_point != ring.cend(); ring_point++) {
    if (bg::comparable_distance(*ring_point, toolpath.front()) < best_distance) {
      best_distance = bg::comparable_distance(*ring_point, toolpath.front());
      best_ring_point = ring_point;
      insert_at_front = true;
    }
    if (bg::comparable_distance(*ring_point, toolpath.back()) < best_distance) {
      best_distance = bg::comparable_distance(*ring_point, toolpath.back());
      best_ring_point = ring_point;
      insert_at_front = false;
    }
  }
  const auto path = insert_at_front ?
                    path_finder(*best_ring_point, toolpath.front()) :
                    path_finder(toolpath.back(), *best_ring_point);
  if (!path) {
    return false;
  }
  toolpath.resize(toolpath.size() + path->size() - 2 + ring.size()); // Make space for the ring and connection.
  auto insertion_point = toolpath.end() - (path->size() - 2 + ring.size()); // Points at the first element after the toolpath.
  if (insert_at_front) {
    std::move_backward(toolpath.begin(), insertion_point, toolpath.end());
    insertion_point = toolpath.begin() + ring.size(); // Leave room for the ring.
  }
  // Now insertion point is where to write the connecting path.  Don't copy the
  // first and last points, they are already part of the toolpath and ring.
  insertion_point = std::copy(path->cbegin()+1, path->cend()-1, insertion_point);
  if (insert_at_front) {
    insertion_point = toolpath.begin();
  }
  // It's a ring so if dir == ANY, we can connect however we like because it
  // won't make a difference.
  if (dir == MillFeedDirection::CONVENTIONAL) {
    // Taken from: http://www.cplusplus.com/reference/algorithm/rotate_copy/
    // Next to take the next of each element because the range is closed at the
    // start and open at the end.
    auto close_ring_point = std::reverse_copy(std::next(ring.cbegin()), std::next(best_ring_point), insertion_point);
    close_ring_point = std::reverse_copy(std::next(best_ring_point), ring.cend(), close_ring_point);
    *close_ring_point = *best_ring_point;
  } else { // It's ANY or CLIMB.  For ANY, we can choose either direction and we
           // default to the current direction.
    auto close_ring_point = std::rotate_copy(ring.cbegin(), best_ring_point, std::prev(ring.cend()), insertion_point);
    *close_ring_point = *best_ring_point;
  }
  // Iff both inputs are reversible than the path remains reversible.
  toolpath_and_allow_reversal.second = dir == MillFeedDirection::ANY && toolpath_and_allow_reversal.second;
  return true;
}

bool attach_ls(const linestring_type_fp& ls,
               pair<linestring_type_fp, bool>& toolpath_and_allow_reversal, // true if the toolpath can be reversed
               const MillFeedDirection::MillFeedDirection& dir,
               const Surface_vectorial::PathFinder& path_finder) {
  auto& toolpath = toolpath_and_allow_reversal.first;
  bool reverse_toolpath; // Do we start with a reversed toolpath?
  bool insert_front = false; // Then, do we insert at the front?
  bool insert_reversed; // Finally, do we reverse the new ls?
  auto best_distance = std::numeric_limits<double>::infinity();
  if (dir != MillFeedDirection::CLIMB) {
    // We may attach it reversed, either:
    // toolpath.front() ... toolpath.back() ls.back() ... ls.front()
    if (bg::distance(toolpath.back(), ls.back()) < best_distance) {
      reverse_toolpath = false;
      insert_front = false;
      insert_reversed = true;
      best_distance = bg::distance(toolpath.back(), ls.back());
    }
    // ls.back() ... ls.front() toolpath.front() ... toolpath.back()
    if (bg::distance(ls.front(), toolpath.front()) < best_distance) {
      reverse_toolpath = false;
      insert_front = true;
      insert_reversed = true;
      best_distance = bg::distance(ls.front(), toolpath.front());
    }
  }
  if (dir != MillFeedDirection::CONVENTIONAL) {
    // We may attach the list in the forward direction, either:
    // toolpath.front() ... toolpath.back() ls.front() ... ls.back()
    if (bg::distance(toolpath.back(), ls.front()) < best_distance) {
      reverse_toolpath = false;
      insert_front = false;
      insert_reversed = false;
      best_distance = bg::distance(toolpath.back(), ls.front());
    }
    // ls.front() ... ls.back() toolpath.front() ... toolpath.back()
    if (bg::distance(ls.back(), toolpath.front()) < best_distance) {
      reverse_toolpath = false;
      insert_front = true;
      insert_reversed = false;
      best_distance = bg::distance(ls.back(), toolpath.front());
    }
  }
  if (toolpath_and_allow_reversal.second) {
    // The toolpath that we are inserting into may be reversed.
    if (dir != MillFeedDirection::CLIMB) {
      // We may attach it reversed, either:
      // toolpath.back() ... toolpath.front() ls.back() ... ls.front()
      if (bg::distance(toolpath.front(), ls.back()) < best_distance) {
        reverse_toolpath = true;
        insert_front = false;
        insert_reversed = true;
        best_distance = bg::distance(toolpath.front(), ls.back());
      }
      // ls.back() ... ls.front() toolpath.back() ... toolpath.front()
      if (bg::distance(ls.front(), toolpath.back()) < best_distance) {
        reverse_toolpath = true;
        insert_front = true;
        insert_reversed = true;
        best_distance = bg::distance(ls.front(), toolpath.back());
      }
    }
    if (dir != MillFeedDirection::CONVENTIONAL) {
      // We may attach the list in the forward direction, either:
      // toolpath.back() ... toolpath.front() ls.front() ... ls.back()
      if (bg::distance(toolpath.front(), ls.front()) < best_distance) {
        reverse_toolpath = true;
        insert_front = false;
        insert_reversed = false;
        best_distance = bg::distance(toolpath.front(), ls.front());
      }
      // ls.front() ... ls.back() toolpath.back() ... toolpath.front()
      if (bg::distance(ls.back(), toolpath.back()) < best_distance) {
        reverse_toolpath = true;
        insert_front = true;
        insert_reversed = false;
        best_distance = bg::distance(ls.back(), toolpath.back());
      }
    }
  }

  if (best_distance == std::numeric_limits<double>::infinity()) {
    return false;
  }
  const auto& toolpath_neighbor = (reverse_toolpath == insert_front) ? toolpath.back() : toolpath.front();
  const auto& ls_neighbor = (insert_front == insert_reversed) ? ls.front() : ls.back();
  const auto path = insert_front ?
                    path_finder(ls_neighbor, toolpath_neighbor) :
                    path_finder(toolpath_neighbor, ls_neighbor);
  if (!path) {
    return false;
  }
  if (reverse_toolpath) {
    bg::reverse(toolpath);
  }
  auto insertion_position = insert_front ? toolpath.begin() : toolpath.end();
  toolpath.insert(insertion_position, path->cbegin() + 1, path->cend() - 1);
  insertion_position = insert_front ? toolpath.begin() : toolpath.end();
  if (insert_reversed) {
    toolpath.insert(insertion_position, ls.crbegin(), ls.crend());
  } else {
    toolpath.insert(insertion_position, ls.cbegin(), ls.cend());
  }
  // Iff both inputs are reversible than the path remains reversible.
  toolpath_and_allow_reversal.second = dir == MillFeedDirection::ANY && toolpath_and_allow_reversal.second;
  return true;
}

void attach_ls(const linestring_type_fp& ls,
               vector<pair<linestring_type_fp, bool>>& toolpaths,
               const MillFeedDirection::MillFeedDirection& dir,
               const Surface_vectorial::PathFinder& path_finder) {
  if (bg::equals(ls.front(), ls.back())) {
    // This path is actually a ring so we can use attach_ring which can connect
    // at any point.
    for (auto& toolpath : toolpaths) {
      if (attach_ring(ls, toolpath, dir, path_finder)) {
        return;
      }
    }
  } else {
    for (auto& toolpath : toolpaths) {
      if (attach_ls(ls, toolpath, dir, path_finder)) {
        return; // Done, we were able to attach to an existing toolpath.
      }
    }
  }
  // If we've reached here, there was no way to attach at all so make a new path.
  if (dir == MillFeedDirection::CONVENTIONAL) {
    toolpaths.push_back(make_pair(linestring_type_fp(ls.crbegin(), ls.crend()), false));
  } else if (dir == MillFeedDirection::CLIMB) {
    toolpaths.push_back(make_pair(linestring_type_fp(ls.cbegin(), ls.cend()), false));
  } else {
    toolpaths.push_back(make_pair(linestring_type_fp(ls.cbegin(), ls.cend()), true)); // true for reversible
  }
}

void attach_mls(const multi_linestring_type_fp& mls,
                vector<pair<linestring_type_fp, bool>>& toolpaths,
                const MillFeedDirection::MillFeedDirection& dir,
                const multi_polygon_type_fp& already_milled_shrunk,
                const Surface_vectorial::PathFinder& path_finder) {
  auto mls_masked = mls - already_milled_shrunk;  // This might chop the single path into many paths.
  mls_masked = eulerian_paths::make_eulerian_paths(mls_masked, dir == MillFeedDirection::ANY, false); // Rejoin those paths as possible.
  for (const auto& ls : mls_masked) { // Maybe more than one if the masking cut one into parts.
    attach_ls(ls, toolpaths, dir, path_finder);
  }
}


static optional<point_type_fp> get_spike(const point_type_fp& prev, const point_type_fp& current, const point_type_fp& next,
                        coordinate_type_fp offset) {
  // Check if this point is making an anti-clockwise turn.
  //https://math.stackexchange.com/a/1324213/96317
  coordinate_type_fp determinant =
      prev.x()*current.y() + prev.y()*next.x() + current.x()*next.y() -
      prev.x()*next.y() - prev.y()*current.x() - current.y()*next.x();
  if (determinant <= 0) {
    return boost::none;
  }
  // Need to add a point.
  // Get the incoming and outgoing vectors.
  auto v_in = current - prev;
  auto v_out = next - current;
  // Rotate them to the right to get the perpendicular vectors at current.
  point_type_fp in_perp{v_in.y(), -v_in.x()};
  point_type_fp out_perp{v_out.y(), -v_out.x()};
  // Normalize each to half the length of the offset and
  // find the sum, which points in the direction for the
  // spike.
  in_perp = in_perp / bg::distance(point_type_fp{0,0}, in_perp);
  out_perp = out_perp / bg::distance(point_type_fp{0,0}, out_perp);
  auto v_dir = (in_perp + out_perp) * offset / 2;
  // Use similar triangles to find the distance to the vertex the vertex on the previous pass.
  auto v_dir_length = bg::distance(point_type_fp{0,0}, v_dir);
  auto distance_to_vertex = offset*offset / v_dir_length;
  auto spike_length = distance_to_vertex - offset;
  // Adjust v_dir to be the spike_length.
  v_dir = v_dir / v_dir_length * spike_length;
  if (!isfinite(v_dir.x()) || !isfinite(v_dir.y())) {
    return boost::none;
  }
  return {current + v_dir};
}

// Find the next point in the ring after ring[index] such that it is
// at least offset away.
static optional<point_type_fp> get_next_point(const linestring_type_fp& ls, size_t index) {
  if (index == ls.size() - 1) {
    return {ls[1]}; // Skip the first one because it's a repeat.
  } else {
    return {ls[index + 1]};
  }
}

// Find the previous point in the ring before ring[index] such that it
// is at least offset away.
static optional<point_type_fp> get_prev_point(const linestring_type_fp& ls, size_t index) {
  if (index == 0) {
    return {ls[ls.size() - 2]}; // Skip the last one because it's a repeat.
  } else {
    return {ls[index - 1]};
  }
}

// Gets all the spikes needed for this ring.
static void add_spikes(ring_type_fp& ring, coordinate_type_fp offset,
                       bool reverse, coordinate_type_fp tolerance,
                       const boost::optional<multi_polygon_type_fp>& spikes_keep_in,
                       const boost::optional<multi_polygon_type_fp>& spikes_keep_out) {
  if (offset == 0 || ring.size() < 3) {
    return;
  }
  // Simplify removes some points and helps when bg::buffer sometimes
  // creates very near points.
  linestring_type_fp ls(ring.cbegin(), ring.cend());
  linestring_type_fp ls_temp;
  bg::simplify(ls, ls_temp, tolerance);
  size_t ring_index = 0;
  // Subtract 1 because the first point is repeated.
  for (size_t i = 0; i < ls_temp.size()-1; i++) {
    // Find the matching point in ring.
    point_type_fp current = ls_temp[i];
    while (current != ring[ring_index]) {
      ring_index++;
    }
    optional<point_type_fp> prev = get_prev_point(ls_temp, i);
    if (!prev) {
      continue;
    }
    optional<point_type_fp> next = get_next_point(ls_temp, i);
    if (!next) {
      continue;
    }
    if (reverse) {
      std::swap(prev, next);
    }
    optional<point_type_fp> spike = get_spike(*prev, current, *next, offset);
    if (spike) {
      // It's possible that our math caused us to make a spike that is too
      // long if the buffer math worked out unfortunately.  Just in case
      // of that, we'll limit the length of the spike so that it won't
      // overlap the previous pass.
      multi_linestring_type_fp masked{linestring_type_fp{current, *spike}};
      if (spikes_keep_out) {
        masked = masked - *spikes_keep_out;
      }
      if (spikes_keep_in) {
        masked = masked & *spikes_keep_in;
      }
      bool found = false;
      for (const auto& ls : masked) {
        if (ls.front() == current) {
          *spike = ls.back();
          found = true;
          break;
        }
        if (ls.back() == current) {
          *spike = ls.front();
          found = true;
          break;
        }
      }
      if (found) {
        ring.insert(ring.begin() + ring_index, {current, *spike});
        ring_index+=2;
      }
    }
  }
}

// Given a ring, attach it to one of the toolpaths.  The ring is first masked
// with the already_milled_shrunk, so it may become a few linestrings.  Those
// linestrings are attached.  Only attach if there is a point on the linestring
// that is close enough to one of the toolpaths' endpoints is it attached.  If
// none of the toolpaths have a close enough endpoint, a new toolpath is added
// to the list of toolpaths.  offset is the tool diameter minus the overlap requested.
void attach_ring(const ring_type_fp& ring,
                 vector<pair<linestring_type_fp, bool>>& toolpaths,
                 const MillFeedDirection::MillFeedDirection& dir,
                 const multi_polygon_type_fp& already_milled_shrunk,
                 const Surface_vectorial::PathFinder& path_finder,
                 const coordinate_type_fp spike_offset,
                 const bool reverse_spikes,
                 const coordinate_type_fp tolerance,
                 const boost::optional<multi_polygon_type_fp>& spikes_keep_in,
                 const boost::optional<multi_polygon_type_fp>& spikes_keep_out) {
  ring_type_fp ring_copy = ring;
  add_spikes(ring_copy, spike_offset, reverse_spikes, tolerance, spikes_keep_in, spikes_keep_out);
  multi_linestring_type_fp ring_paths;
  ring_paths.push_back(linestring_type_fp(ring_copy.cbegin(), ring_copy.cend())); // Make a copy into an mls.
  attach_mls(ring_paths, toolpaths, dir, already_milled_shrunk, path_finder);
}

// Given polygons, attach all the rings inside to the toolpaths.  path_finder is
// the function that can return a path to connect linestrings if such a path is
// possible, as in, not too long and doesn't cross any traces, etc.
void attach_polygons(const multi_polygon_type_fp& polygons,
                     vector<pair<linestring_type_fp, bool>>& toolpaths,
                     const MillFeedDirection::MillFeedDirection& dir,
                     const multi_polygon_type_fp& already_milled_shrunk,
                     const Surface_vectorial::PathFinder& path_finder,
                     const coordinate_type_fp spike_offset,
                     const bool reverse_spikes,
                     const coordinate_type_fp tolerance,
                     const boost::optional<multi_polygon_type_fp>& spikes_keep_in,
                     const boost::optional<multi_polygon_type_fp>& spikes_keep_out) {
  // Loop through the polygons by ring index because that will lead to better
  // connections between loops.
  for (const auto& poly : polygons) {
    attach_ring(poly.outer(), toolpaths, dir, already_milled_shrunk,
                path_finder, spike_offset, reverse_spikes, tolerance,
                spikes_keep_in, spikes_keep_out);
  }
  bool found_one = true;
  for (size_t i = 0; found_one; i++) {
    found_one = false;
    for (const auto& poly : polygons) {
      if (poly.inners().size() > i) {
        found_one = true;
        attach_ring(poly.inners()[i], toolpaths, dir, already_milled_shrunk,
                    path_finder, spike_offset, reverse_spikes, tolerance,
                    spikes_keep_in, spikes_keep_out);
      }
    }
  }
}

Surface_vectorial::PathFinder Surface_vectorial::make_path_finder(
    shared_ptr<RoutingMill> mill,
    const path_finding::PathFindingSurface& path_finding_surface) const {
  return [mill, &path_finding_surface](const point_type_fp& a, const point_type_fp& b) {
           // Solve for distance:
           // risetime at G0 + horizontal distance G0 + plunge G1 ==
           // travel time at G1
           // The horizontal G0 move is for the maximum of the X and Y coordinates.
           const auto vertical_distance = mill->zsafe - mill->zwork;
           const auto max_manhattan = std::max(std::abs(a.x() - b.x()), std::abs(a.y() - b.y()));
           const double horizontalG1speed = mill->feed;
           const double vertG1speed = mill->vertfeed;
           const double g0_time = vertical_distance/mill->g0_vertical_speed + max_manhattan/mill->g0_horizontal_speed + vertical_distance/vertG1speed;
           // The time saved by milling would be g0_time - g1_distance/g1_horizontal_speed.
           // The extra wear on the mill is g1_distance.
           // Wear is limited by the backtrack value (in distance/time).
           // g1_distance/time_saved < backtrack => g1_distance < backtrack/time_saved
           const double max_g1_distance = std::isinf(mill->backtrack) ?
               g0_time * horizontalG1speed :
               mill->backtrack*g0_time / (1 + mill->backtrack/horizontalG1speed);
           return path_finding_surface.find_path(a, b, max_g1_distance, boost::make_optional(mill->path_finding_limit));
         };
}

Surface_vectorial::PathFinderRingIndices Surface_vectorial::make_path_finder_ring_indices(
    shared_ptr<RoutingMill> mill,
    const path_finding::PathFindingSurface& path_finding_surface) const {
  return [mill, &path_finding_surface](const point_type_fp& a, const point_type_fp& b,
                                       path_finding::SearchKey search_key) {
           // Solve for distance:
           // risetime at G0 + horizontal distance G0 + plunge G1 ==
           // travel time at G1
           // The horizontal G0 move is for the maximum of the X and Y coordinates.
           // We'll assume that G0 Z is 50inches/minute and G0 X or Y is 100 in/min, taken from Nomad Carbide 883.
           const auto vertical_distance = mill->zsafe - mill->zwork;
           const auto max_manhattan = std::max(std::abs(a.x() - b.x()), std::abs(a.y() - b.y()));
           const double horizontalG1speed = mill->feed;
           const double vertG1speed = mill->vertfeed;
           const double g0_time = vertical_distance/mill->g0_vertical_speed + max_manhattan/mill->g0_horizontal_speed + vertical_distance/vertG1speed;
           // The time saved by milling would be g0_time - g1_distance/g1_horizontal_speed.
           // The extra wear on the mill is g1_distance.
           // Wear is limited by the backtrack value (in distance/time).
           // g1_distance/time_saved < backtrack => g1_distance < backtrack/time_saved
           const double max_g1_distance = std::isinf(mill->backtrack) ?
               g0_time * horizontalG1speed :
               mill->backtrack*g0_time / (1 + mill->backtrack/horizontalG1speed);
           return path_finding_surface.find_path(a, b, max_g1_distance, mill->path_finding_limit, search_key);
         };
}

// Get all the toolpaths for a single milling bit for just one of the traces or
// thermal holes.  The mill is the tool to use and the tool_diameter and the
// overlap_width are the specifics of the tool to use in the milling.  mirror
// means that the entire shape should be reflected across the x=0 axis, because
// it will be on the back.  The tool_suffix is for making unique filenames if
// there are multiple tools.  The already_milled_shrunk is the running union of
// all the milled area so far, so that new milling can avoid re-milling areas
// that are already milled.  Returns each pass' toolpath with a boolean
// indicating if the path can be reversed.  True means reversal is allowed and
// false means that it isn't.
vector<pair<linestring_type_fp, bool>> Surface_vectorial::get_single_toolpath(
    shared_ptr<RoutingMill> mill, const size_t trace_index, bool mirror, const double tool_diameter,
    const double overlap_width,
    const multi_polygon_type_fp& already_milled_shrunk,
    const path_finding::PathFindingSurface& path_finding_surface) const {
    // This is by how much we will grow each trace if extra passes are needed.
    coordinate_type_fp diameter = tool_diameter;

    shared_ptr<Isolator> isolator = dynamic_pointer_cast<Isolator>(mill);
    // Extra passes are done on each trace if requested,
    // each offset by the tool diameter less the overlap requested.
    int extra_passes;
    coordinate_type_fp overlap = overlap_width;
    if (!isolator) {
      extra_passes = 0;
    } else {
      int computed_extra_passes = int(std::ceil(
          (isolator->isolation_width - tool_diameter) /
          (tool_diameter - overlap_width) - isolator->tolerance)); // In case it divides evenly, do fewer passes.
      if (isolator->extra_passes >= computed_extra_passes) {
        extra_passes = isolator->extra_passes;
      } else {
        extra_passes = computed_extra_passes;
        // The actual overlap that we'll use is such that the final pass
        // will exactly cover the isolation width and no more.
        overlap = tool_diameter - ((isolator->isolation_width - tool_diameter) /
                                   (extra_passes + isolator->tolerance));
      }
    }
    const bool do_voronoi = isolator ? isolator->voronoi : false;

    optional<polygon_type_fp> current_trace = boost::none;
    if (trace_index < vectorial_surface->first.size()) {
      current_trace.emplace(vectorial_surface->first.at(trace_index));
    }
    const auto& current_voronoi = trace_index < voronoi.size() ? voronoi[trace_index] : thermal_holes[trace_index - voronoi.size()];
    const vector<multi_polygon_type_fp> polygons =
        offset_polygon(current_trace, current_voronoi,
                       diameter, overlap, extra_passes + 1, do_voronoi, mill->offset);

    // Find if a distance between two points should be milled or retract, move
    // fast, and plunge.  Milling is chosen if it's faster and also the path is
    // entirely within the path_finding_surface.  If it's not faster or the path
    // isn't possible, boost::none is returned.
    PathFinder path_finder = make_path_finder(mill, path_finding_surface);

    // The rings of polygons are the paths to mill.  The paths may include both
    // inner and outer rings.  They vector has them sorted from the smallest
    // outer to the largest outer, both for voronoi and for regular isolation.
    // Each linestring has a bool attached to it indicating if it is reversible.
    // true means reversal is still allowed.
    vector<pair<linestring_type_fp, bool>> toolpath;
    for (size_t polygon_index = 0; polygon_index < polygons.size(); polygon_index++) {
      const auto& polygon = polygons[polygon_index];
      MillFeedDirection::MillFeedDirection dir = mill_feed_direction;
      if (polygon_index != 0) {
        if (polygon_index + 1 == polygons.size()) {
          // This is the outermost pass and it isn't the only loop so invert
          // it to remove burrs.
          dir = invert(dir);
        } else {
          // This is a middle pass so it can go in any direction.
          dir = MillFeedDirection::ANY;
        }
      }
      if (mirror) {
        // This is on the back so all loops are reversed.
        dir = invert(dir);
      }
      coordinate_type_fp spike_offset;
      boost::optional<multi_polygon_type_fp> spikes_keep_in;
      boost::optional<multi_polygon_type_fp> spikes_keep_out;
      bool reverse_spikes = do_voronoi && trace_index < voronoi.size();
      if (!reverse_spikes) {
        if (polygon_index > 0) {
          spike_offset = diameter - overlap;
          spikes_keep_out = boost::make_optional(multi_polygon_type_fp{polygons[polygon_index - 1]});
          spikes_keep_in = boost::make_optional(multi_polygon_type_fp{current_voronoi});
        } else {
          spike_offset = 0;
        }
      } else {
        // voronoi is done from inside to outward.  The very center
        // voronoi paths are only a half-width apart if the number of
        // passes is even.
        if (extra_passes % 2 == 0) {
          if (polygon_index + 1 < polygons.size()) {
            spike_offset = diameter - overlap;
            spikes_keep_in = boost::make_optional(multi_polygon_type_fp{polygons[polygon_index + 1]});
          } else {
            spike_offset = 0;
          }
        } else {
          if (polygon_index + 1 < polygons.size()) {
            spike_offset = diameter - overlap;
            spikes_keep_in = boost::make_optional(multi_polygon_type_fp{polygons[polygon_index + 1]});
          } else {
            spike_offset = (diameter - overlap)/2;
            spikes_keep_in = boost::make_optional(multi_polygon_type_fp{current_voronoi});
          }
        }
      }
      attach_polygons(polygon, toolpath, dir, already_milled_shrunk, path_finder,
                      spike_offset, reverse_spikes, mill->tolerance,
                      spikes_keep_in, spikes_keep_out);
    }

    return toolpath;
}

// Given a bunch of paths, where some may be one directional, connect
// them if possible.  The second argument in the pair is true iff the
// path is reversible.  Returns new paths to add to the list that was
// provided.
vector<pair<linestring_type_fp, bool>> Surface_vectorial::final_path_finder(
    const std::shared_ptr<RoutingMill>& mill,
    const path_finding::PathFindingSurface& path_finding_surface,
    const vector<pair<linestring_type_fp, bool>>& paths) const {
  // Find all the connectable endpoints.  A connection can only be
  // made if the direction suits it.  connections is the list of
  // possible connections to make.  It is a tuple of (distance between
  // points, point0, point1, path0, path1).  The path indicates an
  // index into the paths, and says which path was the cause for
  // adding the point.  We want to know it so that we don't make
  // connections between paths that are already connected.
  vector<tuple<coordinate_type_fp, point_type_fp, point_type_fp, size_t, size_t>> connections;
  for (size_t i = 0; i < paths.size(); i++) {
    const auto& path1 = paths[i];
    for (size_t j = i+1; j < paths.size(); j++) {
      const auto& path2 = paths[j];
      // We can always do these:
      connections.push_back({bg::distance(path1.first.back(), path2.first.front()),
                             path1.first.back(), path2.first.front(), i, j});
      connections.push_back({bg::distance(path1.first.front(), path2.first.back()),
                             path1.first.back(), path2.first.front(), i, j});
      if (path1.second) {
        // path1 is reversible so we can connect from the front of it.
        connections.push_back({bg::distance(path1.first.front(), path2.first.front()),
                               path1.first.front(), path2.first.front(), i, j});
      }
      if (path2.second) {
        // path2 is reversible so we can connect from the front of it.
        connections.push_back({bg::distance(path1.first.back(), path2.first.back()),
                               path1.first.back(), path2.first.back(), i, j});
      }
    }
  }
  // Sort so that the closest pairs are first.
  std::sort(connections.begin(), connections.end());
  // Find to which polygon each point belongs.  Each one stores an index into all_rind_indices;
  unordered_map<point_type_fp, boost::optional<path_finding::SearchKey>> points_to_poly_id;
  for (const auto& c : connections) {
    if (points_to_poly_id.count(get<1>(c)) == 0) {
      // New point, need to add it.
      points_to_poly_id.emplace(get<1>(c), path_finding_surface.in_surface(get<1>(c)));
    }
    if (points_to_poly_id.count(get<2>(c)) == 0) {
      // New point, need to add it.
      points_to_poly_id.emplace(get<2>(c), path_finding_surface.in_surface(get<2>(c)));
    }
  }

  vector<pair<linestring_type_fp, bool>> new_paths;
  PathFinderRingIndices path_finder = make_path_finder_ring_indices(mill, path_finding_surface);
  DisjointSet<size_t> joined_paths;
  for (const auto& start_end : connections) {
    const point_type_fp& start = get<1>(start_end);
    const point_type_fp& end = get<2>(start_end);
    size_t start_path = get<3>(start_end);
    size_t end_path = get<4>(start_end);
    const auto& start_ring_indices = points_to_poly_id.at(start);
    const auto& end_ring_indices = points_to_poly_id.at(end);
    if (!start_ring_indices ||
        !end_ring_indices ||
        *start_ring_indices != *end_ring_indices) {
      continue;
    }
    if (joined_paths.find(start_path) == joined_paths.find(end_path)) {
      continue; // The two paths were already connected.
    }
    boost::optional<linestring_type_fp> new_path = path_finder(start, end, *start_ring_indices);
    if (new_path) {
      new_paths.push_back({*new_path, true});
      joined_paths.join(start_path, end_path);
    }
  }
  return new_paths;
}

// A bunch of pairs.  Each pair is the tool diameter followed by a vector of paths to mill.
vector<pair<coordinate_type_fp, multi_linestring_type_fp>> Surface_vectorial::get_toolpath(
    shared_ptr<RoutingMill> mill, bool mirror, bool ymirror) {
  bg::unique(vectorial_surface->first);
  for (auto& diameter_and_path : vectorial_surface->second) {
    bg::unique(diameter_and_path.second);
  }
  if (invert_gerbers) {
    vectorial_surface->first = bounding_box - vectorial_surface->first;
  }
  const auto tolerance = mill->tolerance;
  // Get the voronoi region for each trace.
  voronoi = Voronoi::build_voronoi(vectorial_surface->first, bounding_box, tolerance);

  auto isolator = dynamic_pointer_cast<Isolator>(mill);
  if (isolator) {
    if (isolator->preserve_thermal_reliefs && isolator->voronoi) {
      thermal_holes = find_thermal_reliefs(vectorial_surface->first, tolerance);
    }
    const auto tool_count = isolator->tool_diameters_and_overlap_widths.size();
    vector<pair<coordinate_type_fp, multi_linestring_type_fp>> results(tool_count);
    const auto trace_count = vectorial_surface->first.size() + thermal_holes.size(); // Includes thermal holes.
    // One for each trace or thermal hole, including all prior tools.
    vector<multi_polygon_type_fp> already_milled(trace_count);
    for (size_t tool_index = 0; tool_index < tool_count; tool_index++) {
      const auto& tool = isolator->tool_diameters_and_overlap_widths[tool_index];
      const auto tool_diameter = tool.first;
      vector<vector<pair<linestring_type_fp, bool>>> new_trace_toolpaths(trace_count);

      vector<multi_polygon_type_fp> keep_outs;
      keep_outs.reserve(vectorial_surface->first.size());
      for (const auto& poly : vectorial_surface->first) {
        keep_outs.push_back(bg_helpers::buffer(poly, tool_diameter/2 + isolator->offset));
      }
      const auto path_finding_surface = path_finding::PathFindingSurface(mask ? boost::make_optional(mask->vectorial_surface->first) : boost::none, sum(keep_outs), isolator->tolerance);
      for (size_t trace_index = 0; trace_index < trace_count; trace_index++) {
        multi_polygon_type_fp already_milled_shrunk =
            bg_helpers::buffer(already_milled[trace_index], -tool_diameter/2 + tolerance);
        if (tool_index < tool_count - 1) {
          // Don't force isolation.  By pretending that an area around
          // the trace is already milled, it will be removed from
          // consideration for milling.
          if (trace_index < vectorial_surface->first.size()) {
            // This doesn't run for thermal holes.
            multi_polygon_type_fp temp =
                bg_helpers::buffer(vectorial_surface->first.at(trace_index),
                                   tool_diameter/2 + isolator->offset - tolerance);
            already_milled_shrunk = already_milled_shrunk + temp;
          }
        }
        auto new_trace_toolpath = get_single_toolpath(isolator, trace_index, mirror, tool.first, tool.second,
                                                      already_milled_shrunk, path_finding_surface);
        if (invert_gerbers) {
          auto shrunk_bounding_box = bg::return_buffer<box_type_fp>(bounding_box, -isolator->tolerance);
          vector<pair<linestring_type_fp, bool>> temp;
          for (const auto& ls_and_allow_reversal : new_trace_toolpath) {
            multi_linestring_type_fp temp_mls;
            temp_mls = ls_and_allow_reversal.first & shrunk_bounding_box;
            for (const auto& ls : temp_mls) {
              temp.push_back(make_pair(ls, ls_and_allow_reversal.second));
            }
          }
          new_trace_toolpath.swap(temp);
        }
        new_trace_toolpaths[trace_index] = new_trace_toolpath;
        if (tool_index + 1 == tool_count) {
          // No point in updating the already_milled.
          continue;
        }
        multi_linestring_type_fp combined_trace_toolpath;
        combined_trace_toolpath.reserve(new_trace_toolpath.size());
        for (const auto& ls_and_allow_reversal : new_trace_toolpath) {
          combined_trace_toolpath.push_back(ls_and_allow_reversal.first);
        }
        multi_polygon_type_fp new_trace_toolpath_bufferred =
            bg_helpers::buffer(combined_trace_toolpath, tool_diameter/2);
        already_milled[trace_index] = already_milled[trace_index] + new_trace_toolpath_bufferred;
      }

      const string tool_suffix = tool_count > 1 ? "_" + std::to_string(tool_index) : "";
      write_svgs(tool_suffix, tool_diameter, new_trace_toolpaths, isolator->tolerance, tool_index == tool_count - 1);
      auto new_toolpath = flatten(new_trace_toolpaths);
      multi_linestring_type_fp combined_toolpath = post_process_toolpath(mill, boost::make_optional(&path_finding_surface), new_toolpath);
      write_svgs("_final" + tool_suffix, tool_diameter, combined_toolpath, isolator->tolerance, tool_index == tool_count - 1);
      results[tool_index] = make_pair(tool_diameter, mirror_toolpath(combined_toolpath, mirror, ymirror));
    }
    // Now process any lines that need drawing.
    for (const auto& diameter_and_paths : vectorial_surface->second) {
      const auto& tool_diameter = diameter_and_paths.first;
      const auto& paths = diameter_and_paths.second;
      // Each linestring has a bool attached to it indicating if it is reversible.
      // true means reversal is still allowed.
      vector<pair<linestring_type_fp, bool>> new_trace_toolpath;
      PathFinder path_finder =
          [&](const point_type_fp&, const point_type_fp&) -> optional<linestring_type_fp> {
            return boost::none;
          };
      for (const auto& path : paths) {
        attach_ls(path, new_trace_toolpath, MillFeedDirection::ANY, path_finder);
      }
      const string tool_suffix = "_lines_" + std::to_string(tool_diameter);
      write_svgs(tool_suffix, tool_diameter, {new_trace_toolpath}, mill->tolerance, false);
      multi_linestring_type_fp combined_toolpath = post_process_toolpath(isolator, boost::none, new_trace_toolpath);
      results.push_back(make_pair(tool_diameter, mirror_toolpath(combined_toolpath, mirror, ymirror)));
    }
    return results;
  }
  auto cutter = dynamic_pointer_cast<Cutter>(mill);
  if (cutter) {
    const auto path_finding_surface = path_finding::PathFindingSurface(multi_polygon_type_fp(), multi_polygon_type_fp(), cutter->tolerance);
    const auto trace_count = vectorial_surface->first.size();
    vector<vector<pair<linestring_type_fp, bool>>> new_trace_toolpaths(trace_count);

    for (size_t trace_index = 0; trace_index < trace_count; trace_index++) {
      const auto new_trace_toolpath = get_single_toolpath(cutter, trace_index, mirror, cutter->tool_diameter, 0, multi_polygon_type_fp(), path_finding_surface);
      new_trace_toolpaths[trace_index] = new_trace_toolpath;
    }
    write_svgs("", cutter->tool_diameter, new_trace_toolpaths, mill->tolerance, false);
    auto new_toolpath = flatten(new_trace_toolpaths);
    multi_linestring_type_fp combined_toolpath = post_process_toolpath(cutter, boost::none, new_toolpath);
    return {make_pair(cutter->tool_diameter, mirror_toolpath(combined_toolpath, mirror, ymirror))};
  }
  throw std::logic_error("Can't mill with something other than a Cutter or an Isolator.");
}

void Surface_vectorial::save_debug_image(string message)
{
    const string filename = (boost::format("outp%d_%s.svg") % debug_image_index % message).str();
    svg_writer debug_image(build_filename(outputdir, filename), bounding_box);

    ConsistentRand::srand(1);
    debug_image.add(vectorial_surface->first, 1, true);
    for (const auto& diameter_and_path : vectorial_surface->second) {
      debug_image.add(diameter_and_path.second, diameter_and_path.first, true);
    }

    ++debug_image_index;
}

void Surface_vectorial::enable_filling() {
    fill = true;
}

void Surface_vectorial::add_mask(shared_ptr<Surface_vectorial> surface) {
  mask = surface;
  vectorial_surface->first = vectorial_surface->first & mask->vectorial_surface->first;
  for (auto& diameter_and_path : vectorial_surface->second) {
    diameter_and_path.second = diameter_and_path.second & mask->vectorial_surface->first;
  }
}

// The input is the trace which we want to isolate.  It might have
// holes in it.  We might not have an input, which is when we are
// milling for thermal reliefs.  The voronoi is the shape that
// encloses the input and outside which we have no need to mill
// because that will be handled by another call to this function.  The
// diameter is the diameter of the tool and the overlap is by how much
// each pass should overlap the prevoius pass.  Steps is how many
// passes to do, including the first pass.  If do_voronoi is true then
// isolation should be done from the voronoi region inward instead of
// from the trace outward.  The offset is how far to kee away from any
// trace, useful if the milling bit has some diameter that it is
// guaranteed to mill but also some slop that causes it to sometimes
// mill beyond its diameter.  The return value is rings to be milled
// and the number of them matches the number of steps.  The first one
// is always the one closest to the trace.  For both voronoi and for
// regular milling, that means it's the one with the least area.  For
// thermal holes, it would be the one with the most area.
vector<multi_polygon_type_fp> Surface_vectorial::offset_polygon(
    const optional<polygon_type_fp>& input,
    const polygon_type_fp& voronoi_polygon,
    coordinate_type_fp diameter,
    coordinate_type_fp overlap,
    unsigned int steps, bool do_voronoi,
    coordinate_type_fp offset) const {
  // The polygons to add to the PNG debugging output files.
  // Mask the polygon that we need to mill.
  multi_polygon_type_fp milling_poly{do_voronoi ? voronoi_polygon : *input};  // Milling voronoi or trace?
  coordinate_type_fp thermal_offset = 0;
  if (!input) {
    // This means that we are milling a thermal so we need to move inward
    // slightly to accommodate the thickness of the millbit.
    thermal_offset = -diameter/2 - offset;
  }
  // This is the area that the milling must not cross so that it
  // doesn't dig into the trace.  We only need this if there is an
  // input which is not the case if this is a thermal hole.
  multi_polygon_type_fp path_minimum;
  if (input) {
    path_minimum = bg_helpers::buffer(*input, diameter/2 + offset);
  }

  auto voronoi_shrunk = (bg_helpers::buffer(voronoi_polygon, -diameter/2 + overlap/2) + path_minimum) & voronoi_polygon;
  // We need to crop the area that we'll mill if it extends outside the PCB's
  // outline.  This saves time in milling.
  if (mask) {
    milling_poly = milling_poly & mask->vectorial_surface->first;
  } else {
    // Increase the size of the bounding box to accommodate all milling.
    box_type_fp new_bounding_box;
    if (do_voronoi) {
      // This worked experimentally to remove spurious contention.
      double factor = (1-double(steps+2))/2;
      auto expand_by = (diameter - overlap) * factor;
      bg::buffer(bounding_box, new_bounding_box, -expand_by);
      milling_poly = milling_poly & new_bounding_box;
    } else {
      bg::buffer(bounding_box, new_bounding_box, diameter / 2 + (diameter - overlap) * (steps - 1));
    }
  }

  vector<multi_polygon_type_fp> polygons;
  // Convert the input shape into a bunch of rings that need to be milled.
  for (unsigned int i = 0; i < steps; i++) {
    coordinate_type_fp expand_by;
    if (!do_voronoi) {
      // Number of rings is the same as the number of steps.
      expand_by = diameter / 2 + (diameter - overlap) * i;
    } else {
      // Voronoi lines are on the boundary and shared between
      // multi_polygons so we only need half as many of them.
      double factor;
      if (!input) {
        // This means that we are milling a thermal so we need to do all the
        // passes here.  We can't count on the passes around the input surface
        // because there is no input surface.
        factor = -double(i);
      } else {
        factor = ((1-double(steps))/2 + i);
      }
      if (factor > 0) {
        continue; // Don't need this step.
      }
      expand_by = (diameter - overlap) * factor;
    }

    multi_polygon_type_fp buffered_milling_poly = bg_helpers::buffer(milling_poly, expand_by + offset + thermal_offset);
    if (expand_by + offset != 0) {
      if (!do_voronoi) {
        buffered_milling_poly = buffered_milling_poly & voronoi_shrunk;
      } else {
        buffered_milling_poly = buffered_milling_poly + path_minimum;
      }
    }
    if (mask && !bg::covered_by(buffered_milling_poly, mask->vectorial_surface->first)) {
      // Don't mill outside the mask because that's a waste.
      // But don't mill into the trace itself.
      // And don't mill into other traces.
      buffered_milling_poly = ((buffered_milling_poly & mask->vectorial_surface->first) + path_minimum) & voronoi_polygon;
    }
    if (invert_gerbers) {
      buffered_milling_poly = buffered_milling_poly & bounding_box;
    }
    if (polygons.size() > 0 && bg::equals(buffered_milling_poly, polygons.back())) {
      // Once we start getting repeats, we can expect that all the rest will be
      // the same so we're done.
      break;
    }
    polygons.push_back(buffered_milling_poly);
  }

  return polygons;
}
