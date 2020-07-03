#include <vector>
#include <utility>
#include <algorithm>
#include <unordered_map>

#include "geometry.hpp"
#include "bg_operators.hpp"

#include "backtrack.hpp"

namespace backtrack {

using std::vector;
using std::pair;
using std::tuple;
using std::tie;
using std::unordered_map;
using std::set;
using std::make_pair;
using std::priority_queue;
using std::max;
using std::sort;
using std::get;
using std::greater;

struct VertexDegree {
  size_t in;
  size_t out;
  size_t bidi;

  // Returns true if an edge pointing into here would decrease the
  // number of eulerian paths needed.
  bool can_end() const {
    if (out > in + bidi) {
      // There are more out paths than all the possible in paths so this
      // must be a start of some path.
      return true;
    }
    if (in > out + bidi) {
      // We already have too many paths inward.  More would be worse.
      return false;
    }
    // By this point, out - in <= bidi and out - in >= -bidi so
    // abs(out-in) <= bidi If number of unmatched bidi edges is odd then
    // this can be an end.  (bidi - abs(out - in)) % 2 works but we
    // can avoid the abs by just addings.
    return (bidi + out + in) % 2 == 1;
  }

  // Returns true if an edge pointing out of here would decrease the
  // number of eulerian paths needed.
  bool can_start() const {
    if (out > in + bidi) {
      // There are more out paths than all the possible in paths so this
      // can't be the end of some path.
      return false;
    }
    if (in > out + bidi) {
      // Too many inward paths so we could use with another outward path.
      return true;
    }
    // By this point, out - in <= bidi and out - in >= -bidi so
    // abs(out-in) <= bidi If number of unmatched bidi edges is odd then
    // this can be an end.  (bidi - abs(out - in)) % 2 works but we
    // can avoid the abs by just addings.
    return (bidi + out + in) % 2 == 1;
  }
};

// Use Dijkstra's algorithm to find the shortest path from the start
// vertex to any of the vertices in the set of possible end vertices.
// The result is the length of a path and the vector of linestrings
// that make a path from the start to the end vertex, in the order
// that they should be used, in the direction that they should be
// used.
static inline pair<long double, vector<pair<linestring_type_fp, bool>>> find_nearest_vertex(
    const unordered_map<point_type_fp, vector<pair<linestring_type_fp, bool>>>& graph,
    point_type_fp start,
    const unordered_map<point_type_fp, VertexDegree>& vertex_degrees,
    const double g1_speed,
    const double up_time,
    const double g0_speed,
    const double down_time,
    const double in_per_sec) {
  if (!vertex_degrees.at(start).can_start()) {
    // Starting from here isn't useful.
    return {0, {}};
  }
  // unordered_map from vertex to the best-so-far distance to get to the
  // vertex, along with the edge that gets you there.  The edge
  // might be reversed.
  unordered_map<point_type_fp, pair<long double, pair<linestring_type_fp, bool>>> distances;
  distances[start] = make_pair(0, make_pair(linestring_type_fp{}, true));
  priority_queue<pair<long double, point_type_fp>,
                 vector<pair<long double, point_type_fp>>,
                 greater<pair<long double, point_type_fp>>> to_search;
  set<point_type_fp> done;
  to_search.push(make_pair(0, start));
  while (to_search.size() > 0) {
    auto current_vertex = to_search.top().second;
    to_search.pop();
    if (start != current_vertex &&
        vertex_degrees.count(current_vertex) > 0 &&
        vertex_degrees.at(current_vertex).can_end()) {
      // Found the nearest solution.  Return the edges in the right
      // order with the right directionality.
      vector<pair<linestring_type_fp, bool>> reverse_path;
      for (auto v = current_vertex; v != start;) {
        const auto& e = distances.at(v).second;
        reverse_path.emplace_back(e);
        if (e.second && v == e.first.front()) {
          // Bidi edge and it was reversed.
          std::reverse(reverse_path.back().first.begin(), reverse_path.back().first.end());
        }
        v = reverse_path.back().first.front();
      }
      std::reverse(reverse_path.begin(), reverse_path.end());
      return {distances[current_vertex].first, reverse_path};
    }
    if (done.count(current_vertex)) {
      continue; // We already completed this one.
    }
    for (const auto& edge : graph.at(current_vertex)) {
      // Get end that isn't the current_vertex.
      point_type_fp new_vertex = edge.first.back();
      if (edge.second && current_vertex == new_vertex) {
        // Reversible and this was the wrong end.
        new_vertex = edge.first.front();
      }
      if (done.count(new_vertex)) {
        continue;
      }
      long double new_distance = distances[current_vertex].first + bg::length(edge.first);
      const auto max_manhattan = max(abs(new_vertex.x() - start.x()), abs(new_vertex.y() - start.y()));
      double time_with_backtrack = new_distance / g1_speed;
      double time_without_backtrack = up_time + max_manhattan / g0_speed  + down_time;
      double time_saved = time_without_backtrack - time_with_backtrack;
      if (time_saved < 0 || new_distance / time_saved > in_per_sec) {
        continue; // This is already too far away to be useful.
      }
      const auto old_distance_pair = distances.find(new_vertex);
      if (old_distance_pair == distances.cend() || old_distance_pair->second.first > new_distance) {
        distances[new_vertex] = make_pair(new_distance, edge);
      }
      to_search.push(make_pair(distances[new_vertex].first, new_vertex));
    }
    done.insert(current_vertex);
  }
  return {0, {}};
}

// Find paths in the input that, if doubled so that they could be
// traversed twice, would decrease the milling time overall.  The
// input is a list of paths and the reversibility of each one.  The
// output is just the paths that need to be added to decrease the
// overall milling time.
vector<pair<linestring_type_fp, bool>> backtrack(
    const vector<pair<linestring_type_fp, bool>>& paths,
    const double g1_speed, const double up_time, const double g0_speed, const double down_time,
    const double in_per_sec) {
  if (in_per_sec == 0) {
    return {};
  }
  // Convert the paths structure to a unordered_map from vector to edges that
  // meet there.
  unordered_map<point_type_fp, vector<pair<linestring_type_fp, bool>>> graph;
  // Find the in, out, and bidi degree of all vertices.  We can't just
  // use the graph because we will modify this as we go.
  unordered_map<point_type_fp, VertexDegree> vertex_degrees;
  for (const auto& ls : paths) {
    graph[ls.first.front()].push_back(ls);
    graph[ls.first.back()]; // Create this element even if there are no edges from it.
    if (ls.second) {
      // bi-directional
      graph[ls.first.back()].push_back(ls);
      vertex_degrees[ls.first.front()].bidi++;
      vertex_degrees[ls.first.back()].bidi++;
    } else {
      // directional
      vertex_degrees[ls.first.front()].out++;
      vertex_degrees[ls.first.back()].in++;
    }
  }

  vector<pair<linestring_type_fp, bool>> backtracks;
  while (true) {
    // For each odd-degree vertex, find the nearest odd-degree vertex
    // using the distance function on the edge.

    // best_backtracks stores the total length, the start, the end,
    // and a list of paths to take to get there, in the right order
    // and with the right directionality.
    vector<pair<long double, vector<pair<linestring_type_fp, bool>>>> best_backtracks;
    for (const auto& v : vertex_degrees) {
      // Get the length and path to the nearest element.
      // find_nearest_vertex returns 0 if there is none that is close
      // enough or if the start vertex is not can_start(), that is, it
      // has so many paths out already that it shouldn't get anymore.
      auto length_and_path = find_nearest_vertex(graph, v.first, vertex_degrees, g1_speed, up_time, g0_speed, down_time, in_per_sec);
      if (length_and_path.first > 0) {
        best_backtracks.push_back(length_and_path);
      }
    }
    // Now sort so that the shortest backtracks are first.
    sort(best_backtracks.begin(), best_backtracks.end());
    // Select backtracks one-at-a-time, best first.  If the next best
    // to select includes a vertex that should no longer be used
    // because it already has enough inward or outward edges, start
    // the search again.
    auto i = best_backtracks.cbegin();
    for (; i != best_backtracks.cend(); i++) {
      if (vertex_degrees[i->second.front().first.front()].can_start() &&
          vertex_degrees[i->second.back().first.back()].can_end()) {
        for (const auto& p : i->second) {
          backtracks.emplace_back(p);
        }
        if (i->second.front().second) {
          // Start is reversible.
          vertex_degrees[i->second.front().first.front()].bidi++;
        } else {
          // Start is not reversible.
          vertex_degrees[i->second.front().first.front()].out++;
        }
        if (i->second.back().second) {
          // End is reversible.
          vertex_degrees[i->second.back().first.back()].bidi++;
        } else {
          // End is not reversible.
          vertex_degrees[i->second.back().first.back()].in++;
        }
      } else {
        // Can't add this one or any further, begin the search again.
        break;
      }
    }
    if (i == best_backtracks.cend()) {
      return backtracks;
    }
  }
}

} // namespace backtrack
