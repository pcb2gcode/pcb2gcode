#ifndef EULERIAN_PATHS_H
#define EULERIAN_PATHS_H

#include <vector>
#include <map>

#include "geometry.hpp"

namespace eulerian_paths {

static inline bool operator !=(const point_type_fp& x, const point_type_fp& y) {
  return std::tie(x.x(), x.y()) != std::tie(y.x(), y.y());
}

static inline bool operator ==(const point_type_fp& x, const point_type_fp& y) {
  return std::tie(x.x(), x.y()) == std::tie(y.x(), y.y());
}

// Made public for testing.
static inline bool must_start_helper(size_t out_edges, size_t in_edges, size_t bidi_edges) {
  if (out_edges > in_edges + bidi_edges) {
    // Even with all the in and bidi paths, we would still need a path that starts here.
    return true;
  }
  if (in_edges > out_edges + bidi_edges) {
    // Pairing all bidi edges with in edges leaves no edges to start from.
    return false;
  }
  // By this point, out - in <= bidi and out - in >= -bidi so abs(out-in) <=
  // bidi If number of unmatched bidi edges is odd then this must be a start.
  // (bidi - abs(out - in)) % 2 works but we can avoid the abs by just addings.
  return (bidi_edges + out_edges + in_edges) % 2 == 1;
}

/* This finds a minimal number of eulerian paths that cover the input.  The
 * number of paths returned is equal to the number of vertices with odd edge
 * count divided by 2 if all of them are bidirectional.
 *
 * To use, first get paths.  Each path is a vector of n points that
 * represents n-1 line segments.  Each path is considerd
 * bidirectional.
 *
 * After adding paths, build the Eulerian paths.  The resulting paths
 * cover all segments in the input paths with the minimum number of
 * paths as described above.
 */
template <typename point_t, typename linestring_t, typename multi_linestring_t, typename point_less_than_p = std::less<point_t>>
class eulerian_paths {
 public:
  eulerian_paths(const multi_linestring_t& paths, const std::vector<bool>& allow_reversals) :
      paths(paths),
      allow_reversals(allow_reversals) {}
  multi_linestring_t get() {
    /* We use Hierholzer's algorithm to find the minimum cycles.  First, make a
     * path from each vertex with more paths out than in.  In the reversible
     * case, that means an odd path count.  Follow the path until it ends.
     *
     * For the reversible case, the start vertex will have count decrease by
     * one, the end vertex will decrease from 1 to 0, and all other vertices
     * will decrease by two.  So the result is that two vertices will have the
     * edges counts go from odd to even and the rest will have edge counts stay
     * either odd or even.
     *
     * For non-reversible, we only start at vertices where the out edges is
     * greater than in edges.  A path must start at those because any path that
     * goes through will subtract one from both inbound and outbound edge counts
     * so eventually the vertex will end with only outbound eddges and so must
     * be the start of a path.  For the new path, the start vertex will have
     * outbound count decreased by one.  The end path will have inbound
     * decreased by one and outbound must be zero.  All middle vertices will
     * have inbound and outbound decreased by one each.  The result is that the
     * outbound minus inbound count for each vertex stays the same except for
     * the start where it goes one down by one and the end where it goes up by
     * one.  The start was positive because we only started at edges where
     * outbound was greater than inbound and the end must have been negative
     * because outbound is zero.  So both the start and end vertex outbound
     * minus inbound move closer to zero by one.  Doing this on all vertices
     * where outbound is greater than inbound will bring all those vertices to
     * outbound==inbound.  And because the total_outbound==total_inbound, that
     * means that all vertices will have the same number of outbound and
     * inbound, which means that we have made the precondition to stitch_loops.
     */
    add_paths_to_maps();

    multi_linestring_t euler_paths;
    for (const auto& vertex : all_start_vertices) {
      while (must_start(vertex)) {
        // Make a path starting from vertex with odd count.
        linestring_t new_path;
        new_path.push_back(vertex);
        make_path(vertex, &new_path);
        euler_paths.push_back(new_path);
      }
      // The vertex is no longer must_start.  So it must have the same or fewer
      // out edges than in edges, even accounting for bidi edges becoming in
      // edges.  Any path that passes into the vertex will either pass back out,
      // removing one in edge and one out edge, or get stuck because there are
      // zero out edges.  In either case, the number of out edges <= in edges.
    }
    // All vertices have out edges <= in edges.  But total out edges == total in
    // edges so all vertices must have an equal number of out and in edges.  So
    // if we make a path from one, it is sure to end back where it started.
    // We'll go over all our current Euler paths and stitch in loops anywhere
    // that there is an unvisited edge.
    for (auto& euler_path : euler_paths) {
      stitch_loops(&euler_path);
    }

    // Anything remaining is loops on islands.  Make all those paths, too.
    // Prefer directional edges so do those first.
    for (auto start_map : {&start_vertex_to_unvisited_path_index, &bidi_vertex_to_unvisited_path_index}) {
      while (start_map->size() > 0) {
        const auto vertex = start_map->cbegin()->first;
        linestring_t new_path;
        new_path.push_back(vertex);
        make_path(vertex, &new_path);
        // We can stitch right now because all vertices already have even number
        // of edges.
        stitch_loops(&new_path);
        euler_paths.push_back(new_path);
      }
    }

    return euler_paths;
  }

 private:
  bool must_start(const point_t& vertex) const {
    // A vertex must be a starting point if there are more out edges than in
    // edges, even after using the bidi edges.
    auto out_edges = start_vertex_to_unvisited_path_index.count(vertex);
    auto in_edges = end_vertex_to_unvisited_path_index.count(vertex);
    auto bidi_edges = bidi_vertex_to_unvisited_path_index.count(vertex);
    return must_start_helper(out_edges, in_edges, bidi_edges);
  }

  void add_paths_to_maps() {
    // Reset the maps
    start_vertex_to_unvisited_path_index.clear();
    bidi_vertex_to_unvisited_path_index.clear();
    end_vertex_to_unvisited_path_index.clear();
    all_start_vertices.clear();

    for (size_t i = 0; i < paths.size(); i++) {
      auto& path = paths[i];
      if (path.size() < 2) {
        // Valid path must have a start and end.
        continue;
      }
      point_t start = path.front();
      point_t end = path.back();
      all_start_vertices.insert(start);
      if (allow_reversals[i]) {
        bidi_vertex_to_unvisited_path_index.emplace(start, i);
        bidi_vertex_to_unvisited_path_index.emplace(end, i);
        all_start_vertices.insert(end);
      } else {
        start_vertex_to_unvisited_path_index.emplace(start, i);
        end_vertex_to_unvisited_path_index.emplace(end, i);
      }
    }
  }

  // Given a point, make a path from that point as long as possible
  // until a dead end.  Assume that point itself is already in the
  // list.
  void make_path(const point_t& point, linestring_t* new_path) {
    // Find an unvisited path that leads from point.  Prefer out edges to bidi
    // because we may need to save the bidi edges to later be in edges.
    auto vertex_and_path_index = start_vertex_to_unvisited_path_index.find(point);
    auto vertex_to_unvisited_map = &start_vertex_to_unvisited_path_index;
    if (vertex_and_path_index == start_vertex_to_unvisited_path_index.cend()) {
      vertex_and_path_index = bidi_vertex_to_unvisited_path_index.find(point);
      vertex_to_unvisited_map = &bidi_vertex_to_unvisited_path_index;
      if (vertex_and_path_index == bidi_vertex_to_unvisited_path_index.cend()) {
        // No more paths to follow.
        return;
      }
    }
    size_t path_index = vertex_and_path_index->second;
    const auto& path = paths[path_index];
    if (point == path.front()) {
      // Append this path in the forward direction.
      new_path->insert(new_path->end(), path.cbegin()+1, path.cend());
    } else {
      // Append this path in the reverse direction.
      new_path->insert(new_path->end(), path.crbegin()+1, path.crend());
    }
    vertex_to_unvisited_map->erase(vertex_and_path_index); // Remove from the first vertex.
    const point_t& new_point = new_path->back();
    // We're bound to find exactly one unless there is a serious error.
    auto end_map = allow_reversals[path_index] ? &bidi_vertex_to_unvisited_path_index : &end_vertex_to_unvisited_path_index;
    auto range = end_map->equal_range(new_point);
    for (auto iter = range.first; iter != range.second; iter++) {
      if (iter->second == path_index) {
        // Remove the path that ends on the vertex.
        end_map->erase(iter);
        break; // There must be only one.
      }
    }
    // Continue making the path from here.
    make_path(new_point, new_path);
  }

  // Only call this when there are no vertices with uneven edge count.  That
  // means that all vertices must have as many edges leading in as edges leading
  // out.  This can be true if a vertex has no paths at all.  This is also true
  // if some edges are reversable and they could poentially be used to make the
  // number of in edges equal to the number of out edges.  This will traverse a
  // path and, if it finds an unvisited edge, will make a Euler circuit there
  // and stitch it into the current path.  Because all paths have the same
  // number of in and out, the stitch can only possibly end in a loop.  This
  // continues until the end of the path.
  void stitch_loops(linestring_t *euler_path) {
    // Use a counter and not a pointer because the list will grow and pointers
    // may be invalidated.
    linestring_t new_loop;
    for (size_t i = 0; i < euler_path->size(); i++) {
      // Make a path from here.  We don't need the first element, it's already in our path.
      make_path(euler_path->at(i), &new_loop);
      // Did this vertex have any unvisited edges?
      if (new_loop.size() > 0) {
        // Now we stitch it in.
        euler_path->insert(euler_path->begin()+i+1, new_loop.begin(), new_loop.end());
        new_loop.clear(); // Prepare for the next one.
      }
    }
  }

  const multi_linestring_t& paths;
  const std::vector<bool> allow_reversals;
  // Create a map from vertex to each path that start at that vertex.  It's a
  // map to an index into the input paths.
  std::multimap<point_t, size_t, point_less_than_p> start_vertex_to_unvisited_path_index;
  // Create a map from vertex to each bidi path that may start or end at that
  // vertex.  It's a map to an index into the input paths.
  std::multimap<point_t, size_t, point_less_than_p> bidi_vertex_to_unvisited_path_index;
  // Create a map from vertex to each path that may start or end at that vertex.  It's a
  // map to an index into the input paths.
  std::multimap<point_t, size_t, point_less_than_p> end_vertex_to_unvisited_path_index;
  // Only the ones that have at least one potential edge leading out.
  std::set<point_t, point_less_than_p> all_start_vertices;
}; //class eulerian_paths

// Visible for testing only.
template <typename point_t, typename linestring_t, typename multi_linestring_t, typename point_less_than_p = std::less<point_t>>
    multi_linestring_t get_eulerian_paths(const multi_linestring_t& paths, const std::vector<bool>& allow_reversals) {
  return eulerian_paths<point_t, linestring_t, multi_linestring_t, point_less_than_p>(
      paths, allow_reversals).get();
}

// Returns a minimal number of toolpaths that include all the milling in the
// oroginal toolpaths.  Each path is traversed once.  Each path has a bool
// indicating if the path is reversible.
multi_linestring_type_fp make_eulerian_paths(const std::vector<std::pair<linestring_type_fp, bool>>& toolpaths);

multi_linestring_type_fp make_eulerian_paths(const std::vector<linestring_type_fp>& paths, bool reversible);

}; // namespace eulerian_paths
#endif //EULERIAN_PATHS_H
