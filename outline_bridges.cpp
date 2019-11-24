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

#include <boost/container/detail/std_fwd.hpp>  // for pair
#include <boost/geometry.hpp>                  // for distance
#include <algorithm>                           // for sort
#include <iterator>                            // for next
#include <limits>                              // for numeric_limits
#include <map>                                 // for _Rb_tree_const_iterator
#include <set>                                 // for set
#include <utility>                             // for pair

#include "outline_bridges.hpp"

using std::vector;
using std::pair;
using std::shared_ptr;
using std::set;
using std::map;

//This function returns the intermediate point between p0 and p1.
//With position=0 it returns p0, with position=1 it returns p1,
//with values between 0 and 1 it returns the relative position between p0 and p1
icoordpair intermediatePoint(icoordpair p0, icoordpair p1, double position) {
    return icoordpair(p0.first + (p1.first - p0.first) * position,
                      p0.second + (p1.second - p0.second) * position);
}

/* This function takes the segments where the bridges must be built (in the form
 * of set<size_t>, see findBridgeSegments), inserts them in the path and
 * returns an array containing the indexes of each bridge's start.
 */
vector<size_t> insertBridges(shared_ptr<icoords> path, const set<size_t>& bridges, double length) {
  vector<size_t> chosenSegments(bridges.cbegin(), bridges.cend());
  path->reserve(path->size() + chosenSegments.size() * 2);  //Just to avoid unnecessary reallocations
  std::sort(chosenSegments.begin(), chosenSegments.end()); //Sort it (lower index -> higher index)

  vector<size_t> output;
  for(size_t i = 0; i < chosenSegments.size(); i++) {
    //Each time we insert a bridge all following indexes have a offset of 2, we
    auto segment_index = chosenSegments[i] + 2 * i;
    auto segment_start = path->at(segment_index);
    auto segment_end = path->at(segment_index + 1);
    auto segment_length = bg::distance(segment_start, segment_end);
    //Insert the bridges in the path
    path->insert(path->begin() + segment_index + 1,
                 { intermediatePoint(segment_start,
                                     segment_end,
                                     0.5 - (length / segment_length) / 2),
                   intermediatePoint(segment_start,
                                     segment_end,
                                     0.5 + (length / segment_length) / 2) });
    output.push_back(segment_index + 1);    //Add the bridges indexes to output
  }

  return output;
}

// Computes the distance between the two closest points in the clique.  The
// locations of the elements of the clique are in the locations input.  The two
// points that are closest together are set in closest.
static inline double min_clique_distance(const set<size_t>& clique,
                                         const map<size_t, icoordpair>& locations,
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

// Computes the minimum of the distances from the given points to all the other
// points in the clique, not including the same point because it would be distance 0.
static inline vector<double> min_distance_to_clique(icoordpair point,
                                                    const vector<size_t>& excluded_points,
                                                    const set<size_t>& clique,
                                                    const map<size_t, icoordpair>& locations) {
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
 * It starts by finding the longest segments.  Then it iteratively picks one
 * segment at a time to try to replace elsewhere, searching for a swap that will
 * maximize the minimum distance between segments.  It continues until no
 * improvement can be made or until one second has passed.  It may return fewer
 * than the "number" requested if not enough place can be found to place
 * bridges.
 *
 * Returns the positions in path of the segments that need to be modified.
 */
set<size_t> findBridgeSegments(const shared_ptr<icoords> path, size_t number, double length) {
  if (number < 1) {
    return {};
  }

  // All the potential bridge segments and their locations.
  map<size_t, icoordpair> candidates;
  for (size_t i = 0; i < path->size() - 1; i++) {
    auto current_distance = bg::distance(path->at(i), path->at(i+1));
    if (current_distance < length) {
      continue;
    }
    candidates[i] = intermediatePoint(path->at(i),
                                      path->at(i + 1),
                                      0.5);
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
    best_score = min_clique_distance(output, candidates, closest);
  } while(true);
  return output;
}

vector<size_t> outline_bridges::makeBridges(shared_ptr<icoords> &path, size_t number, double length) {
  return insertBridges(path, findBridgeSegments(path, number, length), length);
}
