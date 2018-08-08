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

namespace eulerian_paths {

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
multi_linestring_t get_eulerian_paths(const multi_linestring_t& paths, bool allow_reversal = true) {
  // Create a map from vertex to each path that starts or ends (or
  // both) at that vertex.  It's a map to an input into the input
  // paths.
  std::multimap<point_t, size_t, point_less_than_p> vertex_to_unvisited_path_index;
  // This maps from endpoints to path index and is only used if allow_reversal is false.
  std::multimap<point_t, size_t, point_less_than_p> end_vertex_to_unvisited_path_index;
  for (size_t i = 0; i < paths.size(); i++) {
    auto& path = paths[i];
    if (path.size() < 2) {
      // Valid path must have a start and end.
      continue;
    }
    point_t start = path.front();
    vertex_to_unvisited_path_index.emplace(start, i);
    point_t end = path.back();
    if (allow_reversal) {
      // Put both the front and end into the same list.
      vertex_to_unvisited_path_index.emplace(end, i);
    } else {
      // Put end vertex to the list of end vertices.
      end_vertex_to_unvisited_path_index.emplace(end, i);
    }
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
    if (allow_reversal) {
      // If we added both directions then we're bound to find one, otherwise
      // there is a serious error in the algorithm.
      auto range = vertex_to_unvisited_path_index.equal_range(new_point);
      for (auto iter = range.first; iter != range.second; iter++) {
        if (iter->second == path_index) {
          // Remove the path from the last vertex
          vertex_to_unvisited_path_index.erase(iter);
          break;
        }
      }
    } else {
      // We added the end into a different list so find it there and remove it.
      // We're bound to find exactly one unless there is a serious error.
      auto range = end_vertex_to_unvisited_path_index.equal_range(new_point);
      for (auto iter = range.first; iter != range.second; iter++) {
        if (iter->second == path_index) {
          // Remove the path that ends on the vertex.
          end_vertex_to_unvisited_path_index.erase(iter);
          break;
        }
      }
    }
    // Continue making the path from here.
    make_path(new_point, new_path);
  };

  // Only call this when there are no vertices with uneven edge count.  That
  // means that all vertices must have as many edges leading in as edges leading
  // out.  This can be true if a vertex has no paths at all.  This is also true
  // if paths are reversable and the number of paths is even.  It's also true if
  // the number of paths in equals the number of paths out for non-reversable.
  // This will traverse a path and, if it finds an unvisited edge, will make a
  // Euler circuit there and stitch it into the current path.  Because all paths
  // have the same number of in and out, the stitch can only possibly end in a
  // loop.  This continues until the end of the path.
  auto stitch_loops = [&] (linestring_t *euler_path) -> void {
    // Use a counter and not a pointer because we will add to the list beyond i.
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

  /* We use Hierholzer's algorithm to find the minimum cycles.  First, make a
   * path from each vertex with more paths out than in.  In the reversible case,
   * that means an odd path count.  Follow the path until it ends.
   *
   * For the reversible case, the start vertex will have count decrease by one,
   * the end vertex will decrease from 1 to 0, and all other vertices will
   * decrease by two.  So the result is that two vertices will have the edges
   * counts go from odd to even and the rest will have edge counts stay either
   * odd or even.
   *
   * For non-reversible, we only start at vertices where the out edges is
   * greater than in edges.  A path must start at those because any path that
   * goes through will subtract one from both inbound and outbound edge counts
   * so eventually the vertex will end with only outbound eddges and so must be
   * the start of a path.  For the new path, the start vertex will have outbound
   * count decreased by one.  The end path will have inbound decreased by one
   * and outbound must be zero.  All middle vertices will have inbound and
   * outbound decreased by one each.  The result is that the outbound minus
   * inbound count for each vertex stays the same except for the start where it
   * goes one down by one and thend where it goes up by one.  The start was
   * positive because we only started at edges where outbound was greater than
   * inbound and the end must have been negative because outbound is zero.  So
   * both the start and end vertex outbound minus inbound move closer to zero by
   * one.  Doing this on all vertices where outbound is greater than inbound
   * will bring all those vetices to outbound=inbound.  And because the total
   * outbound==total inbound, that means that all vertices will have the same
   * number of outbound and inbound, which means that we have made the
   * precondition to stich_loops.
   */
  std::vector<point_t> odd_vertices;
  // First find all vertices with more outbound than inbound edges.  For
  // reversible, that means odd count.  For non-reversible, that means the
  // outbound table is bigger than the inbound one.  We do it separate from the
  // loop below because the below loop will modify
  // vertex_to_unvisited_path_index and invalidate the iterator.
  for (auto iter = vertex_to_unvisited_path_index.cbegin();
       iter != vertex_to_unvisited_path_index.cend();) {
    auto& vertex = iter->first;
    if (allow_reversal) {
      if (vertex_to_unvisited_path_index.count(vertex) % 2 == 1) {
        odd_vertices.push_back(vertex);
      }
    } else {
      if (vertex_to_unvisited_path_index.count(vertex) > end_vertex_to_unvisited_path_index.count(vertex)) {
        odd_vertices.push_back(vertex);
      }
    }
    // Advance to a different vertex.
    iter = vertex_to_unvisited_path_index.upper_bound(vertex);
  }

  multi_linestring_t euler_paths{};
  for (const auto& vertex : odd_vertices) {
    // We check again because it might have become even after being connected to
    // a different path in a different call to make_path.
    while ((allow_reversal  && vertex_to_unvisited_path_index.count(vertex) % 2 == 1) ||
           (!allow_reversal && vertex_to_unvisited_path_index.count(vertex) > end_vertex_to_unvisited_path_index.count(vertex))) {
      // Make a path starting from vertex with odd count.
      linestring_t new_path;
      new_path.push_back(vertex);
      make_path(vertex, &new_path);
      euler_paths.push_back(new_path);
    }
  }

  // At this point, all remaining vertices have an even number of
  // edges.  So if we make a path from one, it is sure to end back
  // where it started.  We'll go over all our current Euler paths and
  // stitch in loops anywhere that there is an unvisited edge.
  for (auto& euler_path : euler_paths) {
    stitch_loops(&euler_path);
  }

  // Anything remaining is on islands.  Make all those paths, too.
  while(vertex_to_unvisited_path_index.size() > 0) {
    const auto vertex = vertex_to_unvisited_path_index.cbegin()->first;
    linestring_t new_path;
    new_path.push_back(vertex);
    make_path(vertex, &new_path);
    // We can stitch right now because all vertices already have
    // even number of edges.
    stitch_loops(&new_path);
    euler_paths.push_back(new_path);
  }

  return euler_paths;
}

} //namespace eulerian_paths
#endif //EULERIAN_PATHS_H
