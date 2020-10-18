/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2015 Nicola Corna <nicola@corna.info>
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

#include "outline_bridges.hpp"

#include "bg_operators.hpp"
#include <map>
#include <list>
#include <set>

using std::vector;
using std::pair;
using std::set;
using std::map;
using std::list;

namespace boost { namespace geometry { namespace model { namespace d2 {

static inline bool operator<(const list<point_type_fp>::const_iterator& lhs,
                             const list<point_type_fp>::const_iterator& rhs) {
  return *lhs < *rhs;
}
}}}}

namespace outline_bridges {

//This function returns the intermediate point between p0 and p1.
//With position=0 it returns p0, with position=1 it returns p1, with
//values between 0 and 1 it returns the relative position between p0 and p1
static point_type_fp intermediatePoint(point_type_fp p0, point_type_fp p1, double position) {
  return point_type_fp(p0.x() + (p1.x() - p0.x()) * position,
                       p0.y() + (p1.y() - p0.y()) * position);
}

/* Find where to insert a point that is offset distance away from the
 * point at path[index].  Return the index of the newly inserted
 * point. */
static list<point_type_fp>::const_iterator insertPoint(list<point_type_fp>& path, list<point_type_fp>::const_iterator p, double offset) {
  if (offset == 0) {
    return path.insert(p, *p);
  }
  if (offset < 0) {
    // Backwards.
    if (p != path.cbegin()) {
      auto d = bg::distance(*p, *(std::prev(p)));
      if (offset < -d) {
        // Need to go back beyond the point.
        return insertPoint(path, std::prev(p), offset + d);
      } else {
        auto new_point = intermediatePoint(*p, *(std::prev(p)), -offset/d);
        return path.insert(p, new_point);
      }
    } else {
      // index is 0.
      if (*p == path.back()) {
        return insertPoint(path, std::prev(path.cend()), offset);
      } else {
        // Not a ring so just insert at the start. This might make a
        // bridge that is too short in the rare case where the outline
        // isn't a loop.
        return path.insert(path.begin(), *p);
      }
    }
  } else {
    // Forward.
    if (p != std::prev(path.cend())) {
      auto d = bg::distance(*p, *(std::next(p)));
      if (offset > d) {
        // Need to go forward beyond the point.
        return insertPoint(path, std::next(p), offset - d);
      } else {
        auto new_point = intermediatePoint(*p, *std::next(p), offset/d);
        return path.insert(std::next(p), new_point);
      }
    } else {
      // index is at the end.
      if (*p == path.front()) {
        return insertPoint(path, path.cbegin(), offset);
      } else {
        // Not enough room so just insert at the end.
        return path.insert(path.cend(), *p);
      }
    }
  }
}

/* This function takes the segments where the bridges must be built (in the form
 * of set<size_t>, see findBridgeSegments), inserts them in the path and
 * returns an array containing the indexes of each bridge's start.
 */
static vector<size_t> insertBridges(linestring_type_fp& path, const set<size_t>& bridges, double length) {
  list<point_type_fp> path_list(path.cbegin(), path.cend());
  set<list<point_type_fp>::const_iterator> bridge_pointers;
  {
    auto p = path_list.cbegin();
    size_t i = 0;
    for (; p != path_list.cend(); i++, p++) {
      if (bridges.count(i) > 0) {
        bridge_pointers.insert(p);
      }
    }
  }

  set<list<point_type_fp>::const_iterator> bridge_segments;
  for(const auto& bridge_pointer : bridge_pointers) {
    auto segment_start = *bridge_pointer;
    auto segment_end = *std::next(bridge_pointer);
    auto segment_length = bg::distance(segment_start, segment_end);
    //Insert the bridges in the path.
    auto bridge_start = insertPoint(path_list, bridge_pointer, segment_length/2 - length/2);
    auto bridge_end = insertPoint(path_list, bridge_pointer, segment_length/2 + length/2);
    for (auto seg = bridge_start; seg != bridge_end; seg++) {
      if (seg == path_list.cend()) {
        seg = path_list.cbegin();
      }
      bridge_segments.insert(seg);
    }
  }

  linestring_type_fp new_path;
  vector<size_t> output;
  {
    int i = 0;
    for (auto p = path_list.cbegin(); p != path_list.cend(); p++, i++) {
      new_path.push_back(*p);
      if (bridge_segments.count(p) > 0) {
        output.push_back(i);
      }
    }
  }
  path = new_path;
  return output;
}

// Computes the distance between the two closest points in the clique.  The
// locations of the elements of the clique are in the locations input.  The two
// points that are closest together are set in closest.
static double min_clique_distance(const set<size_t>& clique,
                                  const map<size_t, point_type_fp>& locations,
                                  vector<size_t>& closest) {
  auto current_score = std::numeric_limits<double>::infinity();
  for (auto i = clique.cbegin(); i != clique.cend(); i++) {
    for (auto j = std::next(i); j != clique.cend(); j++) {
      auto distance = boost::geometry::distance(locations.at(*i), locations.at(*j));
      if (distance < current_score) {
        current_score = distance;
        closest[0] = *i;
        closest[1] = *j;
      }
    }
  }
  return current_score;
}

// Computes the minimum of the distances from the given point to all
// the other points in the clique, not including the same point
// because it would be distance 0.
static inline vector<double> min_distance_to_clique(point_type_fp point,
                                                    const vector<size_t>& excluded_points,
                                                    const set<size_t>& clique,
                                                    const map<size_t, point_type_fp>& locations) {
  vector<double> current_score(excluded_points.size(), std::numeric_limits<double>::infinity());
  for (const auto& clique_point : clique) {
    auto distance = boost::geometry::distance(point, locations.at(clique_point));
    for (size_t i = 0; i < excluded_points.size(); i++) {
      if (clique_point == excluded_points[i]) {
        continue;
      }
      if (distance < current_score[i]) {
        current_score[i] = distance;
      }
    }
  }
  return current_score;
}


/* This function finds the segments for putting bridges.
 *
 * It starts by finding the longest segments.  Then it iteratively
 * picks one segment at a time to try to replace elsewhere, searching
 * for a swap that will maximize the minimum distance between
 * segments.  It continues until no improvement can be made.  It may
 * return fewer than the "number" requested if not enough place can be
 * found to place bridges.
 *
 * Returns the positions in path of the segments that need to be modified.
 */
static set<size_t> findBridgeSegments(const linestring_type_fp& path, size_t number, double length) {
  if (number < 1) {
    return {};
  }

  // All the potential bridge segments and their locations.
  map<size_t, point_type_fp> candidates;
  for (size_t i = 0; i < path.size() - 1; i++) {
    auto current_distance = bg::distance(path.at(i), path.at(i+1));
    if (current_distance < length) {
      continue;
    }
    candidates[i] = intermediatePoint(path.at(i),
                                      path.at(i + 1),
                                      0.5);
  }
  if (candidates.size() < number) {
    // We didn't find enough places to put bridges with the length
    // restriction so try again but this time allow small edges, too.
    candidates.clear();
    for (size_t i = 0; i < path.size() - 1; i++) {
      candidates[i] = intermediatePoint(path.at(i),
                                        path.at(i + 1),
                                        0.5);
    }
  }

  // Make a set of bridges that we will output.  They must be unique.  For now
  // just take the first few.
  set<size_t> output;
  for (const auto& candidate : candidates) {
    if (output.size() >= number) {
      break;
    }
    output.insert(candidate.first);
  }

  vector<size_t> closest = {0,0};
  double best_score;
  // Can we do better?

  // Try to improve the score by moving one of the two closest points.
  do {
    best_score = min_clique_distance(output, candidates, closest);
    bool improvement = false;
    auto new_score = best_score;
    size_t swap_from;
    size_t swap_to;
    for (const auto& candidate : candidates) {
      if (output.count(candidate.first) > 0) {
        // This is already in the output so we can't reuse it.
        continue;
      }
      // What happens to the score if we move one of the closest to this new candidate?
      auto current_score = min_distance_to_clique(candidate.second, closest, output, candidates);
      for (size_t i = 0; i < closest.size(); i++) {
        if (current_score[i] > new_score) {
          swap_from = closest[i];
          swap_to = candidate.first;
          new_score = current_score[i];
          improvement = true;
        }
      }
    }
    if (!improvement) {
      break;
    }
    output.erase(swap_from);
    output.insert(swap_to);
  } while(true);
  return output;
}

vector<size_t> makeBridges(linestring_type_fp& path, size_t number, double length) {
  return insertBridges(path, findBridgeSegments(path, number, length), length);
}

} // namespace outline_bridges
