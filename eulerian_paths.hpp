#ifndef EULERIAN_PATHS_H
#define EULERIAN_PATHS_H

#include <vector>
#include <map>
#include <unordered_set>
#include <unordered_map>

#include "geometry.hpp"

namespace eulerian_paths {

template<typename p_t>
struct identity {
  typedef p_t type;
};

enum struct Side : bool {
  front,
  back,
};

static inline Side operator!(const Side& s) {
  switch(s) {
   case Side::front: return Side::back;
   case Side::back: return Side::front;
  }
}

static inline std::ostream& operator<<(std::ostream& out, const Side& s) {
  switch(s) {
   case Side::front:
     out << "front";
     break;
   case Side::back:
     out << "back";
     break;
  }
  return out;
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
template <typename point_t, typename linestring_t>
class eulerian_paths {
 public:
  eulerian_paths(const std::vector<std::pair<linestring_t, bool>>& paths) :
    paths(paths) {}
  std::vector<std::pair<linestring_t, bool>> get() {
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

    std::vector<std::pair<linestring_t, bool>> euler_paths;
    for (const auto& vertex : all_start_vertices) {
      while (must_start(vertex)) {
        // Make a path starting from vertex with odd count.
        linestring_t new_path;
        new_path.push_back(vertex);
        bool reversible = make_path(vertex, &new_path);
        euler_paths.push_back(std::make_pair(new_path, reversible));
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
        std::pair<linestring_t, bool> new_path;
        new_path.first.push_back(vertex);
        bool reversible = make_path(vertex, &(new_path.first));
        new_path.second = reversible;
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
      auto& path = paths[i].first;
      if (path.size() < 2) {
        // Valid path must have a start and end.
        continue;
      }
      point_t start = path.front();
      point_t end = path.back();
      all_start_vertices.insert(start);
      if (paths[i].second) {
        bidi_vertex_to_unvisited_path_index.emplace(start, std::make_pair(i, Side::front));
        bidi_vertex_to_unvisited_path_index.emplace(end, std::make_pair(i, Side::back));
        all_start_vertices.insert(end);
      } else {
        start_vertex_to_unvisited_path_index.emplace(start, std::make_pair(i, Side::front));
        end_vertex_to_unvisited_path_index.emplace(end, std::make_pair(i, Side::back));
      }
    }
  }

  // Higher score is better.
  template <typename p_t>
  double path_score(const linestring_t& path_so_far,
                    const std::pair<point_t, std::pair<size_t, Side>>& option,
                    identity<p_t>) {
    if (path_so_far.size() < 2 || paths[option.second.first].first.size() < 2) {
      // Doesn't matter, pick any.
      return 0;
    }
    auto p0 = path_so_far[path_so_far.size()-2];
    auto p1 = path_so_far.back();
    auto p2 = paths[option.second.first].first[1];
    if (option.second.second == Side::back) {
      // This must be reversed.
      p2 = paths[option.second.first].first[paths[option.second.first].first.size()-2];
    }
    auto length_p0_p1 = bg::distance(p0, p1);
    auto length_p1_p2 = bg::distance(p1, p2);
    auto delta_x = (p1.x() - p0.x())/length_p0_p1 + (p2.x() - p1.x())/length_p1_p2;
    auto delta_y = (p1.y() - p0.y())/length_p0_p1 + (p2.y() - p1.y())/length_p1_p2;
    return (delta_x * delta_x + delta_y * delta_y);  // No need to sqrt, this is comparable.
  }

  double path_score(const linestring_t&,
                    const std::pair<point_t, std::pair<size_t, Side>>&,
                    identity<int>) {
    return 0;
  }

  template <typename p_t>
  double path_score(const linestring_t& path_so_far,
                    const std::pair<point_t, std::pair<size_t, Side>>& option) {
    return path_score(path_so_far, option, identity<p_t>());
  }

  // Pick the best path to continue on given the path_so_far and a
  // range of options.  The range must have at least one element in
  // it.
  typename std::multimap<point_t, std::pair<size_t, Side>>::iterator select_path(
      const linestring_t& path_so_far,
      const std::pair<typename std::multimap<point_t, std::pair<size_t, Side>>::iterator,
                      typename std::multimap<point_t, std::pair<size_t, Side>>::iterator>& options) {
    auto best = options.first;
    double best_score = path_score<point_t>(path_so_far, *best);
    for (auto current = options.first; current != options.second; current++) {
      double current_score = path_score<point_t>(path_so_far, *current);
      if (current_score > best_score) {
        best = current;
        best_score = current_score;
      }
    }
    return best;
  }

  // Given a point, make a path from that point as long as possible
  // until a dead end.  Assume that point itself is already in the
  // list.  Return true if the path is all reversible, otherwise
  // false.
  bool make_path(const point_t& point, linestring_t* new_path) {
    // Find an unvisited path that leads from point.  Prefer out edges to bidi
    // because we may need to save the bidi edges to later be in edges.
    auto vertex_and_path_range = start_vertex_to_unvisited_path_index.equal_range(point);
    auto vertex_to_unvisited_map = &start_vertex_to_unvisited_path_index;
    if (vertex_and_path_range.first == vertex_and_path_range.second) {
      vertex_and_path_range = bidi_vertex_to_unvisited_path_index.equal_range(point);
      vertex_to_unvisited_map = &bidi_vertex_to_unvisited_path_index;
      if (vertex_and_path_range.first == vertex_and_path_range.second) {
        // No more paths to follow.
        return true; // Empty path is reversible.
      }
    }
    auto vertex_and_path_index = select_path(*new_path, vertex_and_path_range);
    size_t path_index = vertex_and_path_index->second.first;
    Side side = vertex_and_path_index->second.second;
    const auto& path = paths[path_index].first;
    if (side == Side::front) {
      // Append this path in the forward direction.
      new_path->insert(new_path->end(), path.cbegin()+1, path.cend());
    } else {
      // Append this path in the reverse direction.
      new_path->insert(new_path->end(), path.crbegin()+1, path.crend());
    }
    vertex_to_unvisited_map->erase(vertex_and_path_index); // Remove from the first vertex.
    const point_t& new_point = new_path->back();
    // We're bound to find exactly one unless there is a serious error.
    auto end_map = paths[path_index].second ? &bidi_vertex_to_unvisited_path_index : &end_vertex_to_unvisited_path_index;
    auto range = end_map->equal_range(new_point);
    for (auto iter = range.first; iter != range.second; iter++) {
      if (iter->second == std::make_pair(path_index, !side)) {
        // Remove the path that ends on the vertex.
        end_map->erase(iter);
        break; // There must be only one.
      }
    }
    // Continue making the path from here.
    return make_path(new_point, new_path) && paths[path_index].second;
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
  void stitch_loops(std::pair<linestring_t, bool> *euler_path) {
    // Use a counter and not a pointer because the list will grow and pointers
    // may be invalidated.
    linestring_t new_loop;
    for (size_t i = 0; i < euler_path->first.size(); i++) {
      // Make a path from here.  We don't need the first element, it's already in our path.
      bool new_loop_reversible = make_path(euler_path->first[i], &new_loop);
      // Did this vertex have any unvisited edges?
      if (new_loop.size() > 0) {
        // Now we stitch it in.
        euler_path->first.insert(euler_path->first.begin()+i+1, new_loop.begin(), new_loop.end());
        euler_path->second = euler_path->second && new_loop_reversible;
        new_loop.clear(); // Prepare for the next one.
      }
    }
  }

  const std::vector<std::pair<linestring_t, bool>>& paths;
  // Create a map from vertex to each path that start at that vertex.
  // It's a map to an index into the input paths.  The bool tells us
  // if the point_t is at the front or back.  For start, it will
  // always be true.
  std::multimap<point_t, std::pair<size_t, Side>> start_vertex_to_unvisited_path_index;
  // Create a map from vertex to each bidi path that may start or end
  // at that vertex.  It's a map to an index into the input paths.
  // The bool tells us if the point_t is at the front or back.  For
  // bidi, it could be either.
  std::multimap<point_t, std::pair<size_t, Side>> bidi_vertex_to_unvisited_path_index;
  // Create a map from vertex to each path that may start or end at
  // that vertex.  It's a map to an index into the input paths.  The
  // bool tells us if the point_t is at the front or back.  For end,
  // it will always be false.
  std::multimap<point_t, std::pair<size_t, Side>> end_vertex_to_unvisited_path_index;
  // Only the ones that have at least one potential edge leading out.
  std::set<point_t> all_start_vertices;
}; //class eulerian_paths

// Returns a minimal number of toolpaths that include all the milling in the
// oroginal toolpaths.  Each path is traversed once.  Each path has a bool
// indicating if the path is reversible.
template <typename point_t, typename linestring_t>
std::vector<std::pair<linestring_t, bool>> get_eulerian_paths(const std::vector<std::pair<linestring_t, bool>>& paths) {
  return eulerian_paths<point_t, linestring_t>(
      paths).get();
}

multi_linestring_type_fp make_eulerian_paths(const std::vector<linestring_type_fp>& paths, bool reversible);

} // namespace eulerian_paths
#endif //EULERIAN_PATHS_H
