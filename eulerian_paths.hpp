#ifndef EULERIAN_PATHS_H
#define EULERIAN_PATHS_H

#include <vector>
#include <map>

#include "geometry.hpp"

/* This finds a minimal number of eulerian paths that cover the input.
 * The number of paths returned is equal to the number of vertices
 * with odd edge count divided by 2.
 *
 * To use, first get paths.  Each path is a vector of n points that
 * represents n-1 line segments.  Each path is considerd
 * bidirectional.
 *
 * After adding paths, build the Eulerian paths.  The resulting paths
 * cover all segments in the input paths with the minimum number of
 * paths as described above.
 */

struct PointLessThan {
  bool operator()(const point_type& a, const point_type& b) const {
    return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
  }
};

bool operator !=(const point_type& x, const point_type& y) {
  return std::tie(x.x(), x.y()) != std::tie(y.x(), y.y());
}


template <typename point_p, typename point_less_than_p = std::less<point_p>>
std::vector<std::vector<point_p>> get_eulerian_paths(std::vector<std::vector<point_p>> paths) {
  // Create a map from vertex to each path that starts or ends (or
  // both) at that vertex.  It's a map to an input into the input
  // paths.
  std::multimap<point_p, size_t, point_less_than_p> vertex_to_path_index;
  for (size_t i = 0; i < paths.size(); i++) {
    auto& path = paths[i];
    if (path.size() == 0) {
      continue;
    }
    point_p start = path.front();
    point_p end = path.back();
    vertex_to_path_index.emplace(start, i);
    if (start != end) {
      vertex_to_path_index.emplace(end, i);
    }
  }
  std::vector<bool> path_visited(paths.size()); // At first, no paths have been visited.

  // We use Hierholzer's algorithm to find the minimum cycles.
  // First, make a path from each vertex with odd path count until the path ends.
  point_p& previous_vertex = paths[0][0];  // Doesn't matter what we put here.
  int unvisited_count = 0;
  for (auto& vertex_and_path_index : vertex_to_path_index) {
    auto& vertex = vertex_and_path_index.first;
    if (vertex != previous_vertex) {
      unvisited_count = 0;
      previous_vertex = vertex;
    }
    auto& path_index = vertex_and_path_index.second;
    if (!path_visited[path_index]) {
      unvisited_count++;
    }
    printf("done!\n");
  }
  return std::vector<std::vector<point_p>>{};
}

#endif //EULERIAN_PATHS_H
