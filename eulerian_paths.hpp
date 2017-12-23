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

bool operator !=(const point_type& x, const point_type& y) {
  return std::tie(x.x(), x.y()) != std::tie(y.x(), y.y());
}

bool operator ==(const point_type& x, const point_type& y) {
  return std::tie(x.x(), x.y()) == std::tie(y.x(), y.y());
}

bool operator !=(const point_type_fp& x, const point_type_fp& y) {
  return std::tie(x.x(), x.y()) != std::tie(y.x(), y.y());
}

bool operator ==(const point_type_fp& x, const point_type_fp& y) {
  return std::tie(x.x(), x.y()) == std::tie(y.x(), y.y());
}

template <typename point_t, typename linestring_t, typename multi_linestring_t, typename point_less_than_p = std::less<point_t>>
multi_linestring_t get_eulerian_paths(const multi_linestring_t& paths) {
  // Create a map from vertex to each path that starts or ends (or
  // both) at that vertex.  It's a map to an input into the input
  // paths.
  std::multimap<point_t, size_t, point_less_than_p> vertex_to_unvisited_path_index;
  for (size_t i = 0; i < paths.size(); i++) {
    auto& path = paths[i];
    if (path.size() < 2) {
      // Valid path must have a start and end.
      continue;
    }
    point_t start = path.front();
    point_t end = path.back();
    vertex_to_unvisited_path_index.emplace(start, i);
    vertex_to_unvisited_path_index.emplace(end, i);
  }

  // Given a point, make a path from that point as long as possible
  // until a dead end.  Assume that point itself is already in the
  // list.
  std::function<void(const point_t&, linestring_t*)> make_path = [&] (const point_t& point, linestring_t* new_path) -> void {
    // Find an unvisited path that leads from point, any will do.
    auto vertex_and_path_index = vertex_to_unvisited_path_index.find(point);
    if (vertex_and_path_index == vertex_to_unvisited_path_index.end()) {
      // No more paths to follow.
      return;
    }
    size_t path_index = vertex_and_path_index->second;
    auto& path = paths[path_index];
    if (point == path.front()) {
      // Append this path in the forward direction.
      new_path->insert(new_path->end(), path.cbegin()+1, path.cend());
    } else {
      // Append this path in the reverse direction.
      new_path->insert(new_path->end(), path.crbegin()+1, path.crend());
    }
    vertex_to_unvisited_path_index.erase(vertex_and_path_index); // Remove from the first vertex.
    point_t& new_point = new_path->back();
    // We're bound to find one, otherwise there is a serious error in
    // the algorithm.
    auto range = vertex_to_unvisited_path_index.equal_range(new_point);
    for (auto iter = range.first; iter != range.second; iter++) {
      if (iter->second == path_index) {
        // Remove the path from the last vertex
        vertex_to_unvisited_path_index.erase(iter);
        break;
      }
    }
    // Continue making the path from here.
    make_path(new_point, new_path);
  };

  // Only call this when there are no vertices with odd edge count.
  // This will traverse a path and, if it finds an unvisited edge,
  // will make a Euler circuit there and stitch it into the current
  // path.  This continues until the end of the path.
  auto stitch_loops = [&] (linestring_t *euler_path) -> void {
    for (size_t i = 0; i < euler_path->size(); i++) {
      // Does this vertex have any unvisited edges?
      if (vertex_to_unvisited_path_index.count((*euler_path)[i]) > 0) {
        // Make a path from here.  We don't need the first element, it's already in our path.
        linestring_t new_loop{};
        make_path((*euler_path)[i], &new_loop);
        // Now we stitch it in.
        euler_path->insert(euler_path->begin()+i+1, new_loop.begin(), new_loop.end());
      }
    }
  };

  // We use Hierholzer's algorithm to find the minimum cycles.  First,
  // make a path from each vertex with odd path count until the path
  // ends.
  multi_linestring_t euler_paths{};
  for (auto iter = vertex_to_unvisited_path_index.cbegin(); iter != vertex_to_unvisited_path_index.cend();) {
    auto& vertex = iter->first;
    if (vertex_to_unvisited_path_index.count(vertex) % 2 == 1) {
      // Make a path starting from vertex with odd count.
      linestring_t new_path{vertex};
      make_path(vertex, &new_path);
      euler_paths.push_back(new_path);
    }
    // Advance to a different vertex.
    iter = vertex_to_unvisited_path_index.upper_bound(vertex);
  }

  // At this point, all remaining vertices have an even number of
  // edges.  So if we make a path from one, it is sure to end back
  // where it started.  We'll go over all our current Euler paths and
  // stitch in loops anywhere that there is an unvisited edge.
  for (auto& euler_path : euler_paths) {
    stitch_loops(&euler_path);
  }

  // Anything remaining is on islands.  Make all those paths, too.  No
  // stitching needed because all vertices have an even number of
  // edges.
  while(vertex_to_unvisited_path_index.size() > 0) {
    const auto vertex = vertex_to_unvisited_path_index.cbegin()->first;
    linestring_t new_path{vertex};
    make_path(vertex, &new_path);
    // We can stitch right now because all vertices already have
    // even number of edges.
    stitch_loops(&new_path);
    euler_paths.push_back(new_path);
  }

  return euler_paths;
}

#endif //EULERIAN_PATHS_H
