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
#include <algorithm>                           // for sort
#include <queue>                               // for priority_queue
#include <utility>                             // for pair, make_pair
#include "boost/container/detail/std_fwd.hpp"  // for pair
#include "boost/geometry.hpp"                  // for distance

using std::vector;
using std::pair;
using std::shared_ptr;

//This function returns the intermediate point between p0 and p1.
//With position=0 it returns p0, with position=1 it returns p1,
//with values between 0 and 1 it returns the relative position between p0 and p1
icoordpair intermediatePoint(icoordpair p0, icoordpair p1, double position) {
    return icoordpair(p0.first + (p1.first - p0.first) * position,
                      p0.second + (p1.second - p0.second) * position);
}

/* This function takes the segments where the bridges must be built (in the form
 * of vector<pair<uint,double>>, see findLongestSegments), inserts them in the
 * path and returns an array containing the indexes of each bridge's start.
 */
vector<size_t> insertBridges(shared_ptr<icoords> path, vector<pair<size_t, double>> chosenSegments, double length) {
  path->reserve(path->size() + chosenSegments.size() * 2);  //Just to avoid unnecessary reallocations
  std::sort(chosenSegments.begin(), chosenSegments.end()); //Sort it (lower index -> higher index)

  vector<size_t> output;
  for(size_t i = 0; i < chosenSegments.size(); i++) {
    //Each time we insert a bridge all following indexes have a offset of 2, we compensate it
    chosenSegments[i].first += 2 * i;
    icoords temp{
      intermediatePoint(path->at(chosenSegments[i].first),
                        path->at(chosenSegments[i].first + 1),
                        0.5 - (length / chosenSegments[i].second) / 2),
      intermediatePoint(path->at(chosenSegments[i].first),
                        path->at(chosenSegments[i].first + 1),
                        0.5 + (length / chosenSegments[i].second) / 2)
    };
    //Insert the bridges in the path
    path->insert(path->begin() + chosenSegments[i].first + 1, temp.begin(), temp.end());
    output.push_back(chosenSegments[i].first + 1);    //Add the bridges indexes to output
  }

  return output;
}

/* This function finds the longest segments and returns a vector of pair
 * containing the index of path where the segment starts and its length. It may
 * return fewer than the "number" requested if not enough place can be found to
 * place bridges.
 */
vector<pair<size_t, double>> findLongestSegments(const shared_ptr<icoords> path, size_t number, double length) {
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
    auto current_distance = boost::geometry::distance(path->at(i), path->at(i+1));
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

  vector<pair<size_t, double>> output;
  while (!distances.empty()) {
    output.push_back(distances.top());
    distances.pop();
  }
  return output;
}

vector<size_t> outline_bridges::makeBridges(shared_ptr<icoords> &path, size_t number, double length) {
  return insertBridges(path, findLongestSegments(path, number, length), length);
}
