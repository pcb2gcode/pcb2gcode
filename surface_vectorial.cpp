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

#include <map>
using std::map;

#include <boost/format.hpp>
#include <boost/optional.hpp>
using boost::optional;
using boost::make_optional;

#include <glibmm/miscutils.h>
using Glib::build_filename;

#include "tsp_solver.hpp"
#include "surface_vectorial.hpp"
#include "eulerian_paths.hpp"
#include "segmentize.hpp"
#include "bg_helpers.hpp"
#include "units.hpp"
#include "path_finding.hpp"
#include "merge_near_points.hpp"

using std::max;
using std::max_element;
using std::next;
using std::dynamic_pointer_cast;

unsigned int Surface_vectorial::debug_image_index = 0;

// For use when we have to convert from float to long and back.
const double SCALE = 1000000.0;

Surface_vectorial::Surface_vectorial(unsigned int points_per_circle,
                                     ivalue_t min_x, ivalue_t max_x,
                                     ivalue_t min_y, ivalue_t max_y,
                                     string name, string outputdir,
                                     bool tsp_2opt, MillFeedDirection::MillFeedDirection mill_feed_direction,
                                     bool invert_gerbers, bool render_paths_to_shapes) :
    points_per_circle(points_per_circle),
    bounding_box(box_type_fp(point_type_fp(min_x, min_y),
                             point_type_fp(max_x, max_y))),
    name(name),
    outputdir(outputdir),
    tsp_2opt(tsp_2opt),
    fill(false),
    mill_feed_direction(mill_feed_direction),
    invert_gerbers(invert_gerbers),
    render_paths_to_shapes(render_paths_to_shapes) {}

void Surface_vectorial::render(shared_ptr<GerberImporter> importer) {
  auto vectorial_surface_not_simplified = importer->render(fill, render_paths_to_shapes, points_per_circle);

  if (bg::intersects(vectorial_surface_not_simplified.first)) {
    cerr << "\nWarning: Geometry of layer '" << name << "' is"
        " self-intersecting. This can cause pcb2gcode to produce"
        " wildly incorrect toolpaths. You may want to check the"
        " g-code output and/or fix your gerber files!\n";
  }

  //With a very small loss of precision we can reduce memory usage and processing time
  vectorial_surface = make_shared<
      pair<multi_polygon_type_fp, map<coordinate_type_fp, multi_linestring_type_fp>>>();
  bg::simplify(vectorial_surface_not_simplified.first, vectorial_surface->first, 0.0001);
  for (const auto& diameter_and_path : vectorial_surface_not_simplified.second) {
    vectorial_surface->second[diameter_and_path.first] = multi_linestring_type_fp();
    bg::simplify(diameter_and_path.second,
                 vectorial_surface->second[diameter_and_path.first],
                 0.0001);
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

multi_linestring_type_fp mirror_toolpath(
    const multi_linestring_type_fp& mls, bool mirror) {
  multi_linestring_type_fp result;
  for (const auto& ls : mls) {
    linestring_type_fp new_ls;
    for (const auto& point : ls) {
      new_ls.push_back(
          point_type_fp(
              (mirror ? -point.x() : point.x()),
              point.y()));
    }
    result.push_back(new_ls);
  }
  return result;
}

vector<shared_ptr<icoords>> mls_to_icoords(const multi_linestring_type_fp& mls) {
  vector<shared_ptr<icoords>> result;
  for (const auto& ls : mls) {
    result.push_back(make_shared<icoords>());
    for (const auto& p : ls) {
      result.back()->push_back(icoordpair(p.x(), p.y()));
    }
  }
  return result;
}

// Find all potential thermal reliefs.  Those are usually holes in traces.
// Return those shapes as rings with correct orientation.
vector<polygon_type_fp> find_thermal_reliefs(const multi_polygon_type_fp& milling_surface,
                                             const coordinate_type_fp tolerance) {
  // For each shape, see if it has any holes that are empty.
  optional<svg_writer> image;
  vector<polygon_type_fp> holes;
  for (const auto& p : milling_surface) {
    for (const auto& inner : p.inners()) {
      auto thermal_hole = inner;
      bg::correct(thermal_hole); // Convert it from a hole to a filled-in shape.
      multi_polygon_type_fp shrunk_thermal_hole;
      bg_helpers::buffer(thermal_hole, shrunk_thermal_hole, -tolerance);
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

vector<pair<linestring_type_fp, bool>> flatten_mls(const vector<vector<pair<linestring_type_fp, bool>>>& v) {
  vector<pair<linestring_type_fp, bool>> result;
  for (const auto& sub : v) {
    result.insert(result.end(), sub.begin(), sub.end());
  }
  return result;
}

void Surface_vectorial::write_svgs(const string& tool_suffix, coordinate_type_fp tool_diameter,
                                   const vector<vector<pair<linestring_type_fp, bool>>>& new_trace_toolpaths,
                                   coordinate_type_fp tolerance, bool find_contentions) const {
  // Now set up the debug images, one per tool.
  svg_writer debug_image(build_filename(outputdir, "processed_" + name + tool_suffix + ".svg"), bounding_box);
  svg_writer traced_debug_image(build_filename(outputdir, "traced_" + name + tool_suffix + ".svg"), bounding_box);
  optional<svg_writer> contentions_image;
  srand(1);
  debug_image.add(voronoi, 0.2, false);
  srand(1);
  const auto trace_count = new_trace_toolpaths.size();
  for (size_t trace_index = 0; trace_index < trace_count; trace_index++) {
    const auto& new_trace_toolpath = new_trace_toolpaths[trace_index];
    const unsigned int r = rand() % 256;
    const unsigned int g = rand() % 256;
    const unsigned int b = rand() % 256;
    for (const auto& ls_and_allow_reversal : new_trace_toolpath) {
      debug_image.add(ls_and_allow_reversal.first, tool_diameter, r, g, b);
      traced_debug_image.add(ls_and_allow_reversal.first, tool_diameter, r, g, b);
    }

    if (find_contentions) {
      if (trace_index < vectorial_surface->first.size()) {
        multi_polygon_type_fp temp;
        bg_helpers::buffer(vectorial_surface->first.at(trace_index), temp, tool_diameter/2 - tolerance);
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
  srand(1);
  debug_image.add(vectorial_surface->first, 1, true);
  for (const auto& diameter_and_path : vectorial_surface->second) {
    debug_image.add(diameter_and_path.second, diameter_and_path.first, 1, true);
  }
}

// Returns a minimal number of toolpaths that include all the milling in the
// oroginal toolpaths.  Each path is traversed once.  First paths are
// directional, second are bidi.  In the pair, the first is directional and the
// second is bidi.
multi_linestring_type_fp make_eulerian_paths(const vector<pair<linestring_type_fp, bool>>& toolpaths) {
  // Merge points that are very close to each other because it makes
  // us more likely to find intersections that was can use.
  auto merged_toolpaths = toolpaths;
  merge_near_points(merged_toolpaths, 0.00001);

  // First we need to split all paths so that they don't cross.  We need to
  // scale them up because the input is not floating point.
  vector<segment_type_p> all_segments;
  vector<bool> allow_reversals;
  for (const auto& toolpath_and_allow_reversal : merged_toolpaths) {
    const auto& toolpath = toolpath_and_allow_reversal.first;
    for (size_t i = 1; i < toolpath.size(); i++) {
      all_segments.push_back(
          segment_type_p(
              point_type_p(toolpath[i-1].x() * SCALE, toolpath[i-1].y() * SCALE),
              point_type_p(toolpath[i  ].x() * SCALE, toolpath[i  ].y() * SCALE)));
      allow_reversals.push_back(toolpath_and_allow_reversal.second);
    }
  }
  vector<std::pair<segment_type_p, bool>> split_segments = segmentize::segmentize(all_segments, allow_reversals);

  // Make a minimal number of paths from those segments.
  struct PointLessThan {
    bool operator()(const point_type_fp& a, const point_type_fp& b) const {
      return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
    }
  };
  // Only allow reversing the direction of travel if mill_feed_direction is
  // ANY.  We need to scale them back down.
  multi_linestring_type_fp segments_as_linestrings;
  segments_as_linestrings.reserve(split_segments.size());
  allow_reversals.clear();
  allow_reversals.reserve(split_segments.size());
  for (const auto& segment_and_allow_reversal : split_segments) {
    // Make a little 1-edge linestrings.
    linestring_type_fp ls;
    const auto& segment = segment_and_allow_reversal.first;
    const auto& allow_reversal = segment_and_allow_reversal.second;
    ls.push_back(point_type_fp(segment.low().x() / SCALE, segment.low().y() / SCALE));
    ls.push_back(point_type_fp(segment.high().x() / SCALE, segment.high().y() / SCALE));
    segments_as_linestrings.push_back(ls);
    allow_reversals.push_back(allow_reversal);
  }

  return eulerian_paths::get_eulerian_paths<
      point_type_fp,
      linestring_type_fp,
      multi_linestring_type_fp,
      PointLessThan>(segments_as_linestrings, allow_reversals);
}

// Make eulerian paths if needed.  Sort the paths order to make it faster.
// Simplify paths by remving points that don't affect the path or affect it very
// little.
multi_linestring_type_fp Surface_vectorial::post_process_toolpath(
    const std::shared_ptr<RoutingMill>& mill, const vector<pair<linestring_type_fp, bool>>& toolpath) const {
  multi_linestring_type_fp combined_toolpath;
  if (mill->eulerian_paths) {
    combined_toolpath = make_eulerian_paths(toolpath);
  } else {
    combined_toolpath.reserve(toolpath.size());
    for (const auto& ls_and_allow_reversal : toolpath) {
      combined_toolpath.push_back(ls_and_allow_reversal.first);
    }
  }
  if (tsp_2opt) {
    tsp_solver::tsp_2opt(combined_toolpath, point_type_fp(0, 0));
  } else {
    tsp_solver::nearest_neighbour(combined_toolpath, point_type_fp(0, 0));
  }

  if (mill->optimise) {
    multi_linestring_type_fp temp_mls;
    bg::simplify(combined_toolpath, temp_mls, mill->tolerance);
    combined_toolpath = temp_mls;
  }
  return combined_toolpath;
}

// This function returns a linestring that connects two points if possible.
typedef std::function<optional<linestring_type_fp>(const point_type_fp& start, const point_type_fp& end)> PathFinder;

// Given a linestring which has the same front and back (so it's actually a
// ring), attach it to one of the ends of the toolpath.  Only attach if there is
// a point on the ring that is close enough to the toolpath endpoint.  toolpath
// must not be empty.
bool attach_ring(const linestring_type_fp& ring,
                 pair<linestring_type_fp, bool>& toolpath_and_allow_reversal, // true if the toolpath can be reversed
                 const MillFeedDirection::MillFeedDirection& dir,
                 const PathFinder& path_finder) {
  auto& toolpath = toolpath_and_allow_reversal.first;
  bool insert_at_front = true;
  auto best_ring_point = ring.begin();
  double best_distance = bg::comparable_distance(*best_ring_point, toolpath.front());
  for (auto ring_point = ring.begin(); ring_point != ring.end(); ring_point++) {
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
  insertion_point = std::copy(path->begin()+1, path->end()-1, insertion_point);
  if (insert_at_front) {
    insertion_point = toolpath.begin();
  }
  // It's a ring so if dir == ANY, we can connect however we like because it
  // won't make a difference.
  if (dir == MillFeedDirection::CONVENTIONAL) {
    // Taken from: http://www.cplusplus.com/reference/algorithm/rotate_copy/
    // Next to take the next of each element because the range is closed at the
    // start and open at the end.
    auto close_ring_point = std::reverse_copy(std::next(ring.begin()), std::next(best_ring_point), insertion_point);
    close_ring_point = std::reverse_copy(std::next(best_ring_point), ring.end(), close_ring_point);
    *close_ring_point = *best_ring_point;
  } else { // It's ANY or CLIMB.  For ANY, we can choose either direction and we
           // default to the current direction.
    auto close_ring_point = std::rotate_copy(ring.begin(), best_ring_point, std::prev(ring.end()), insertion_point);
    *close_ring_point = *best_ring_point;
  }
  // Iff both inputs are reversible than the path remains reversible.
  toolpath_and_allow_reversal.second = dir == MillFeedDirection::ANY && toolpath_and_allow_reversal.second;
  return true;
}

bool attach_ls(const linestring_type_fp& ls,
               pair<linestring_type_fp, bool>& toolpath_and_allow_reversal, // true if the toolpath can be reversed
               const MillFeedDirection::MillFeedDirection& dir,
               const PathFinder& path_finder) {
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
               const multi_polygon_type_fp& already_milled_shrunk,
               const PathFinder& path_finder) {
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
        return;
      }
    }
  }
  if (dir == MillFeedDirection::CONVENTIONAL) {
    toolpaths.push_back(make_pair(linestring_type_fp(ls.crbegin(), ls.crend()), false));
  } else if (dir == MillFeedDirection::CLIMB) {
    toolpaths.push_back(make_pair(linestring_type_fp(ls.cbegin(), ls.cend()), false));
  } else {
    toolpaths.push_back(make_pair(linestring_type_fp(ls.cbegin(), ls.cend()), true)); // true for reversible
  }
}

// Given a ring, attach it to one of the toolpaths.  The ring is first masked
// with the already_milled_shrunk, so it may become a few linestrings.  Those
// linestrings are attached.  Only attach if there is a point on the linestring
// that is close enough to one of the toolpaths' endpoints is it attached.  If
// none of the toolpaths have a close enough endpoint, a new toolpath is added
// to the list of toolpaths.
void attach_ring(const ring_type_fp& ring,
                 vector<pair<linestring_type_fp, bool>>& toolpaths,
                 const MillFeedDirection::MillFeedDirection& dir,
                 const multi_polygon_type_fp& already_milled_shrunk,
                 const PathFinder& path_finder) {
  multi_linestring_type_fp ring_paths;
  ring_paths.push_back(linestring_type_fp(ring.cbegin(), ring.cend())); // Make a copy into an mls.
  ring_paths = ring_paths - already_milled_shrunk;  // This might chop the single path into many paths.
  vector<pair<linestring_type_fp, bool>> ring_paths_with_direction;
  for (const auto& ring_path : ring_paths) {
    ring_paths_with_direction.push_back(make_pair(ring_path, dir == MillFeedDirection::ANY));
  }
  ring_paths = make_eulerian_paths(ring_paths_with_direction); // Rejoin those paths as possible.
  for (const auto& ring_path : ring_paths) { // Maybe more than one if the masking cut one into parts.
    attach_ls(ring_path, toolpaths, dir, already_milled_shrunk, path_finder);
  }
}

// Given polygons, attach all the rings inside to the toolpaths.  path_finder is
// the function that can return a path to connect linestrings if such a path is
// possible, as in, not too long and doesn't cross any traces, etc.
void attach_polygons(const multi_polygon_type_fp& polygons,
                     vector<pair<linestring_type_fp, bool>>& toolpaths,
                     const MillFeedDirection::MillFeedDirection& dir,
                     const multi_polygon_type_fp& already_milled_shrunk,
                     const PathFinder& path_finder) {
  // Loop through the polygons by ring index because that will lead to better
  // connections between loops.
  for (const auto& poly : polygons) {
    attach_ring(poly.outer(), toolpaths, dir, already_milled_shrunk, path_finder);
  }
  bool found_one = true;
  for (size_t i = 0; found_one; i++) {
    found_one = false;
    for (const auto& poly : polygons) {
      if (poly.inners().size() > i) {
        found_one = true;
        attach_ring(poly.inners()[i], toolpaths, dir, already_milled_shrunk, path_finder);
      }
    }
  }
}

// Get all the toolpaths for a single milling bit for just one of the traces or
// thermal holes.  The mill is the tool to use and the tool_diameter and the
// overlap_width are the specifics of the tool to use in the milling.  mirror
// means that the entire shapre should be reflected across the y=0 axis, because
// it will be on the back.  The tool_suffix is for making unique filenames if
// there are multiple tools.  The already_milled_shrunk is the running union of
// all the milled area so far, so that new milling can avoid re-milling areas
// that are already milled.  Returns each pass' toolpath with a boolean
// indicating if the path can be reversed.  True means reversal is allowed and
// false means that it isn't.
vector<pair<linestring_type_fp, bool>> Surface_vectorial::get_single_toolpath(
    shared_ptr<RoutingMill> mill, const size_t trace_index, bool mirror, const double tool_diameter,
    const double overlap_width,
    const multi_polygon_type_fp& already_milled_shrunk) const {
    // This is by how much we will grow each trace if extra passes are needed.
    coordinate_type_fp diameter = tool_diameter;
    coordinate_type_fp overlap = overlap_width;

    shared_ptr<Isolator> isolator = dynamic_pointer_cast<Isolator>(mill);
    // Extra passes are done on each trace if requested,
    // each offset by the tool diameter less the overlap requested.
    const int extra_passes =
        isolator
        ? std::max(
            isolator->extra_passes,
            int(std::ceil(
                (isolator->isolation_width - tool_diameter) /
                (tool_diameter - overlap_width)
                - isolator->tolerance))) // In case it divides evenly, do fewer passes.
        : 0;
    const bool do_voronoi = isolator ? isolator->voronoi : false;

    optional<polygon_type_fp> current_trace = boost::none;
    if (trace_index < vectorial_surface->first.size()) {
      current_trace.emplace(vectorial_surface->first.at(trace_index));
    }
    const auto& current_voronoi = trace_index < voronoi.size() ? voronoi[trace_index] : thermal_holes[trace_index - voronoi.size()];
    const vector<multi_polygon_type_fp> polygons =
        offset_polygon(current_trace, current_voronoi,
                       diameter, overlap, extra_passes + 1, do_voronoi);
    multi_polygon_type_fp keep_in;
    keep_in.push_back(current_voronoi);
    optional<multi_polygon_type_fp> keep_out;
    if (current_trace) {
      keep_out = bg_helpers::buffer(*current_trace, diameter/2);
    }
    auto path_finding_surface = path_finding::create_path_finding_surface(keep_in, keep_out, mill->tolerance);
    // Find if a distance between two ponts should be milled or retract, move
    // fast, and plunge.  Milling is chosen if it's faster and also the path is
    // entirely within the path_finding_surface.  If it's not faster or the path
    // isn't possible, boost::none is returned.
    PathFinder path_finder =
        [&](const point_type_fp& a, const point_type_fp& b) {
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
          const double max_g1_distance = g0_time * horizontalG1speed;
          size_t tries = 0;
          path_finding::PathLimiter path_limiter =
              [&](const point_type_fp& waypoint, const coordinate_type_fp& length_so_far) -> bool {
                if (tries >= mill->path_finding_limit) {
                  throw path_finding::GiveUp();
                }
                tries++;
                // Return true if this path needs to be clipped.  The distance from
                // a to target so far is length.  At best, we'll have a stright
                // line from there to the goal, b.
                const auto potential_horizontal_distance = length_so_far + bg::distance(waypoint, b);
                if (potential_horizontal_distance > max_g1_distance) {
                  return true; // clip this path
                }
                return false;
              };
          return find_path(path_finding_surface, a, b, path_limiter);
        };

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
      attach_polygons(polygon, toolpath, dir, already_milled_shrunk, path_finder);
    }

    return toolpath;
}

// A bunch of pairs.  Each pair is the tool diameter followed by a vector of paths to mill.
vector<pair<coordinate_type_fp, vector<shared_ptr<icoords>>>> Surface_vectorial::get_toolpath(
    shared_ptr<RoutingMill> mill, bool mirror) {
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
    vector<pair<coordinate_type_fp, vector<shared_ptr<icoords>>>> results(tool_count);
    const auto trace_count = vectorial_surface->first.size() + thermal_holes.size(); // Includes thermal holes.
    // One for each trace or thermal hole, including all prior tools.
    vector<multi_polygon_type_fp> already_milled(trace_count);
    for (size_t tool_index = 0; tool_index < tool_count; tool_index++) {
      const auto& tool = isolator->tool_diameters_and_overlap_widths[tool_index];
      const auto tool_diameter = tool.first;
      vector<vector<pair<linestring_type_fp, bool>>> new_trace_toolpaths(trace_count);

      for (size_t trace_index = 0; trace_index < trace_count; trace_index++) {
        multi_polygon_type_fp already_milled_shrunk;
        bg_helpers::buffer(already_milled[trace_index], already_milled_shrunk, -tool_diameter/2 + tolerance);
        if (tool_index < tool_count - 1) {
          // Don't force isolation.
          if (trace_index < vectorial_surface->first.size()) {
            multi_polygon_type_fp temp;
            bg_helpers::buffer(vectorial_surface->first.at(trace_index), temp, tool_diameter/2 - mill->tolerance);
            already_milled_shrunk = already_milled_shrunk + temp;
          }
        }
        auto new_trace_toolpath = get_single_toolpath(isolator, trace_index, mirror, tool.first, tool.second,
                                                      already_milled_shrunk);
        if (invert_gerbers) {
          auto shrunk_bounding_box = bg::return_buffer<box_type_fp>(bounding_box, -mill->tolerance);
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
        if (mill->optimise) {
          for (auto& ls_and_allow_reversal : new_trace_toolpath) {
            linestring_type_fp temp_ls;
            bg::simplify(ls_and_allow_reversal.first, temp_ls, mill->tolerance);
            ls_and_allow_reversal.first = temp_ls;
          }
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
        multi_polygon_type_fp new_trace_toolpath_bufferred;
        bg_helpers::buffer(combined_trace_toolpath, new_trace_toolpath_bufferred, tool_diameter/2);
        already_milled[trace_index] = already_milled[trace_index] + new_trace_toolpath_bufferred;
      }
      const string tool_suffix = tool_count > 1 ? "_" + std::to_string(tool_index) : "";
      write_svgs(tool_suffix, tool_diameter, new_trace_toolpaths, mill->tolerance, tool_index == tool_count - 1);
      auto new_toolpath = flatten_mls(new_trace_toolpaths);
      multi_linestring_type_fp combined_toolpath = post_process_toolpath(isolator, new_toolpath);
      results[tool_index] = make_pair(tool_diameter, mls_to_icoords(mirror_toolpath(combined_toolpath, mirror)));
    }
    // Now process any lines that need drawing.
    for (const auto& diameter_and_paths : vectorial_surface->second) {
      const auto& tool_diameter = diameter_and_paths.first;
      const auto& paths = diameter_and_paths.second;
      // Each linestring has a bool attached to it indicating if it is reversible.
      // true means reversal is still allowed.
      vector<pair<linestring_type_fp, bool>> new_trace_toolpath;
      PathFinder path_finder =
          [&](const point_type_fp& a, const point_type_fp& b) -> optional<linestring_type_fp> {
            return boost::none;
          };
      for (const auto& path : paths) {
        attach_ls(path, new_trace_toolpath, MillFeedDirection::ANY, multi_polygon_type_fp(), path_finder);
      }
      if (mill->optimise) {
        for (auto& ls_and_allow_reversal : new_trace_toolpath) {
          linestring_type_fp temp_ls;
          bg::simplify(ls_and_allow_reversal.first, temp_ls, mill->tolerance);
          ls_and_allow_reversal.first = temp_ls;
        }
      }
      const string tool_suffix = "_lines_" + std::to_string(tool_diameter);
      write_svgs(tool_suffix, tool_diameter, {new_trace_toolpath}, mill->tolerance, false);
      multi_linestring_type_fp combined_toolpath = post_process_toolpath(isolator, new_trace_toolpath);
      results.push_back(make_pair(tool_diameter, mls_to_icoords(mirror_toolpath(combined_toolpath, mirror))));
    }
    return results;
  }
  auto cutter = dynamic_pointer_cast<Cutter>(mill);
  if (cutter) {
    const auto trace_count = vectorial_surface->first.size();
    vector<vector<pair<linestring_type_fp, bool>>> new_trace_toolpaths(trace_count);

    for (size_t trace_index = 0; trace_index < trace_count; trace_index++) {
      const auto new_trace_toolpath = get_single_toolpath(cutter, trace_index, mirror, cutter->tool_diameter, 0, multi_polygon_type_fp());
      new_trace_toolpaths[trace_index] = new_trace_toolpath;
    }
    write_svgs("", cutter->tool_diameter, new_trace_toolpaths, mill->tolerance, false);
    auto new_toolpath = flatten_mls(new_trace_toolpaths);
    multi_linestring_type_fp combined_toolpath = post_process_toolpath(cutter, new_toolpath);
    return {make_pair(cutter->tool_diameter, mls_to_icoords(mirror_toolpath(combined_toolpath, mirror)))};
  }
  throw std::logic_error("Can't mill with something other than a Cutter or an Isolator.");
}

void Surface_vectorial::save_debug_image(string message)
{
    const string filename = (boost::format("outp%d_%s.svg") % debug_image_index % message).str();
    svg_writer debug_image(build_filename(outputdir, filename), bounding_box);

    srand(1);
    debug_image.add(vectorial_surface->first, 1, true);
    for (const auto& diameter_and_path : vectorial_surface->second) {
      debug_image.add(diameter_and_path.second, diameter_and_path.first, 1, true);
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

// Might not have an input, which is when we are milling for thermal reliefs.
vector<multi_polygon_type_fp> Surface_vectorial::offset_polygon(
    const optional<polygon_type_fp>& input,
    const polygon_type_fp& voronoi_polygon,
    coordinate_type_fp diameter,
    coordinate_type_fp overlap,
    unsigned int steps, bool do_voronoi) const {
  // The polygons to add to the PNG debuging output files.
  vector<multi_polygon_type_fp> polygons;

  // Mask the polygon that we need to mill.
  multi_polygon_type_fp masked_milling_poly;
  masked_milling_poly.push_back(do_voronoi ? voronoi_polygon : *input);  // Milling voronoi or trace?
  if (!input) {
    // This means that we are milling a thermal so we need to move inward
    // slightly to accomodate the thickness of the millbit.
    multi_polygon_type_fp temp;
    bg_helpers::buffer(masked_milling_poly, temp, -diameter/2);
    masked_milling_poly = temp;
  }
  // This is the area that the milling must not cross so that it doesn't dig
  // into the trace.  We only need this if there is an input. which is not the
  // case if this is a thermal hole.
  multi_polygon_type_fp path_minimum;
  if (input) {
    bg_helpers::buffer(*input, path_minimum, diameter/2);
  }

  multi_polygon_type_fp masked_milling_polys;
  // We need to crop the area that we'll mill if it extends outside the PCB's
  // outline.  This saves time in milling.
  if (mask) {
    masked_milling_polys = masked_milling_poly & mask->vectorial_surface->first;
  } else {
    // Increase the size of the bounding box to accomodate all milling.
    box_type_fp new_bounding_box;
    if (do_voronoi) {
      // This worked experimentally to remove spurious contention.
      double factor = (1-double(steps+2))/2;
      auto expand_by = (diameter - overlap) * factor;
      bg::buffer(bounding_box, new_bounding_box, -expand_by);
    } else {
      bg::buffer(bounding_box, new_bounding_box, diameter / 2 + (diameter - overlap) * (steps - 1));
    }
    masked_milling_polys = masked_milling_poly & new_bounding_box;
  }

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
        factor = double(i) + 1 - steps;
      } else {
        factor = ((1-double(steps))/2 + i);
      }
      if (factor > 0) {
        continue; // Don't need this step.
      }
      expand_by = (diameter - overlap) * factor;
    }

    multi_polygon_type_fp mpoly;
    if (expand_by == 0) {
      // We simply need to mill every ring in the shape.
      mpoly = masked_milling_polys;
    } else {
      multi_polygon_type_fp mpoly_temp;
      // Buffer should be done on floating point polygons.
      bg_helpers::buffer(masked_milling_polys, mpoly_temp, expand_by);

      if (!do_voronoi) {
        mpoly = mpoly_temp & voronoi_polygon;
      } else {
        mpoly = mpoly_temp + path_minimum;
      }
    }
    multi_polygon_type_fp masked_expanded_milling_polys;
    if (mask) {
      // Don't mill outside the mask because that's a waste.
      // But don't mill into the trace itself.
      // And don't mill into other traces.
      masked_expanded_milling_polys = ((mpoly & mask->vectorial_surface->first) + path_minimum) & voronoi_polygon;
    } else {
      masked_expanded_milling_polys = mpoly;
    }
    if (invert_gerbers) {
      masked_expanded_milling_polys = masked_expanded_milling_polys & bounding_box;
    }
    if (polygons.size() > 0 && bg::equals(masked_expanded_milling_polys, polygons.back())) {
      // Once we start getting repeats, we can expect that all the rest will be
      // the same so we're done.
      break;
    }
    polygons.push_back(masked_expanded_milling_polys);
  }

  return polygons;
}

svg_writer::svg_writer(string filename, box_type_fp bounding_box) :
    output_file(filename),
    bounding_box(bounding_box)
{
    const coordinate_type_fp width =
        (bounding_box.max_corner().x() - bounding_box.min_corner().x()) * SVG_PIX_PER_IN;
    const coordinate_type_fp height =
        (bounding_box.max_corner().y() - bounding_box.min_corner().y()) * SVG_PIX_PER_IN;
    const coordinate_type_fp viewBox_width =
        (bounding_box.max_corner().x() - bounding_box.min_corner().x()) * SVG_DOTS_PER_IN;
    const coordinate_type_fp viewBox_height =
        (bounding_box.max_corner().y() - bounding_box.min_corner().y()) * SVG_DOTS_PER_IN;

    //Some SVG readers does not behave well when viewBox is not specified
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"0 0 %3% %4%\"") % width % height % viewBox_width % viewBox_height);

    mapper = unique_ptr<bg::svg_mapper<point_type_fp>>
        (new bg::svg_mapper<point_type_fp>(output_file, viewBox_width, viewBox_height, svg_dimensions));
    mapper->add(bounding_box);
}

template <typename multi_polygon_type_t>
void svg_writer::add(const multi_polygon_type_t& geometry, double opacity, bool stroke)
{
    string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:2" : "";

    for (const auto& poly : geometry)
    {
        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;

        multi_polygon_type_t new_bounding_box;
        bg::convert(bounding_box, new_bounding_box);

        mapper->map(poly & new_bounding_box,
            str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);" + stroke_str) %
            opacity % r % g % b));
    }
}

void svg_writer::add(const multi_linestring_type_fp& mls, coordinate_type_fp width, double opacity, bool stroke) {
  string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:2" : "";

  for (const auto& ls : mls) {
    const unsigned int r = rand() % 256;
    const unsigned int g = rand() % 256;
    const unsigned int b = rand() % 256;

    add(ls, width, r, g, b);
  }
}

void svg_writer::add(const linestring_type_fp& path, coordinate_type_fp width, unsigned int r, unsigned int g, unsigned int b) {
  // Stroke the width of the path.
  mapper->map(path,
              str(boost::format("stroke:rgb(%u,%u,%u);stroke-width:%f;fill:none;"
                                "stroke-opacity:0.5;stroke-linecap:round;stroke-linejoin:round;") % r % g % b % (width * SVG_DOTS_PER_IN)));
  // Stroke the center of the path.
  mapper->map(path,
              "stroke:rgb(0,0,0);stroke-width:1px;fill:none;"
              "stroke-opacity:1;stroke-linecap:round;stroke-linejoin:round;");
}

void svg_writer::add(const multi_linestring_type_fp& path, coordinate_type_fp width, unsigned int r, unsigned int g, unsigned int b) {
  for (const auto& p : path) {
    add(p, width, r, g, b);
  }
}
