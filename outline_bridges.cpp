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

#include <boost/assign/list_of.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <cmath>
#include <queue>
#include <set>

using std::vector;
using std::pair;
using std::shared_ptr;
using std::set;

//This function returns the intermediate point between p0 and p1.
//With position=0 it returns p0, with position=1 it returns p1,
//with values between 0 and 1 it returns the relative position between p0 and p1
icoordpair intermediatePoint(icoordpair p0, icoordpair p1, double position) {
    return icoordpair(p0.first + (p1.first - p0.first) * position,
                      p0.second + (p1.second - p0.second) * position);
}

/* This function takes the segments where the bridges must be built (in the form
 * of set<size_t>, see findLongestSegments), inserts them in the path and
 * returns an array containing the indexes of each bridge's start.
 */
vector<size_t> insertBridges(shared_ptr<icoords> path, const set<size_t>& bridges, double length) {
  vector<size_t> chosenSegments(bridges.cbegin(), bridges.cend());
  path->reserve(path->size() + chosenSegments.size() * 2);  //Just to avoid unnecessary reallocations
  std::sort(chosenSegments.begin(), chosenSegments.end()); //Sort it (lower index -> higher index)

  vector<size_t> output;
  for(size_t i = 0; i < chosenSegments.size(); i++) {
    //Each time we insert a bridge all following indexes have a offset of 2, we compensate it
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

/* This function finds the longest segments and returns a vector of pair
 * containing the index of path where the segment starts and its length. It may
 * return fewer than the "number" requested if not enough place can be found to
 * place bridges.
 */
set<size_t> findLongestSegments(const shared_ptr<icoords> path, size_t number, double length) {
  if (number < 1) {
    return {};
  }
  struct greater_2nd {
    bool operator()(const pair<size_t, double>& a, const pair<size_t, double>& b) {
      return a.second > b.second;
    }
  };
  // distances.top() will be the smallest element in the list when sorted by
  // second element of the pair.
  std::priority_queue<pair<size_t, double>,
                      vector<pair<size_t, double>>,
                      greater_2nd> distances;

  for (size_t i = 0; i < path->size() - 1; i++) {
    auto current_distance = bg::distance(path->at(i), path->at(i+1));
    if (current_distance < length) {
      continue;
    }
    if (distances.size() < number || current_distance > distances.top().second) {
      distances.push(std::make_pair(i, current_distance));
    }
    if (distances.size() > number) {
      distances.pop();
    }
  }

  set<size_t> output;
  while (!distances.empty()) {
    output.insert(distances.top().first);
    distances.pop();
  }
  return output;
}

vector<size_t> outline_bridges::makeBridges(shared_ptr<icoords> &path, size_t number, double length) {
  return insertBridges(path, findLongestSegments(path, number, length), length);
}
