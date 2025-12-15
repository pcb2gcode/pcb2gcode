#ifndef EULERIAN_PATHS_H
#define EULERIAN_PATHS_H

#include <vector>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <boost/optional.hpp>

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

/* This class holds on to all the paths and uses std::multimap internally to
 * make it quick to look up which paths extend from a given vertex and in which
 * direction. */
template <typename point_t, typename linestring_t>
class path_manager {
 public:
  // The bool indicates if the path is reversible.
  path_manager(const std::vector<std::pair<linestring_t, bool>>& paths) : paths(paths) {
    // Add all the paths to the maps.
    for (size_t i = 0; i < paths.size(); i++) {
      auto& path = paths[i].first;
      if (path.size() < 2) {
        // Valid path must have a start and end.
        continue;
      }
      auto& reversible = paths[i].second;
      point_t start = path.front();
      point_t end = path.back();
      if (reversible) {
        all_bidi.emplace(start, i);
        all_bidi.emplace(end, i);
      } else {
        all_start.emplace(start, i);
        all_end.emplace(end, i);
      }
      all_endpoints.emplace(start);
      all_endpoints.emplace(end);
    }
  }
  // Return all the endpoints that have at least one unvisited path connected.
  auto get_all_endpoints() const {
    return all_endpoints;
  }
  auto count_unvisited_outgoing_paths(const point_t& vertex) const {
    return all_start.count(vertex);
  }
  auto count_unvisited_incoming_paths(const point_t& vertex) const {
    return all_end.count(vertex);
  }
  auto count_unvisited_bidi_paths(const point_t& vertex) const {
    return all_bidi.count(vertex);
  }
  auto get_unvisited_outgoing_paths(const point_t& vertex) const {
    return all_start.equal_range(vertex);
  }
  auto get_unvisited_incoming_paths(const point_t& vertex) const {
    return all_end.equal_range(vertex);
  }
  auto get_unvisited_bidi_paths(const point_t& vertex) const {
    return all_bidi.equal_range(vertex);
  }
  const std::pair<linestring_t, bool>& get_path(size_t index) const {
    return paths[index];
  }
  void remove_path(size_t index) {
    const auto& path = paths[index].first;
    const auto& reversible = paths[index].second;
    point_t start = path.front();
    point_t end = path.back();
    
    // Remove from the appropriate multimap
    if (reversible) {
      // Remove from all_bidi for both start and end
      auto range = all_bidi.equal_range(start);
      for (auto it = range.first; it != range.second; it++) {
        if (it->second == index) {
          all_bidi.erase(it);
          break;
        }
      }
      range = all_bidi.equal_range(end);
      for (auto it = range.first; it != range.second; it++) {
        if (it->second == index) {
          all_bidi.erase(it);
          break;
        }
      }
    } else {
      // Remove from all_start
      auto range = all_start.equal_range(start);
      for (auto it = range.first; it != range.second; it++) {
        if (it->second == index) {
          all_start.erase(it);
          break;
        }
      }
      // Remove from all_end
      range = all_end.equal_range(end);
      for (auto it = range.first; it != range.second; it++) {
        if (it->second == index) {
          all_end.erase(it);
          break;
        }
      }
    }
  }
 private:
  const std::vector<std::pair<linestring_t, bool>>& paths;
  // A map from each endpoint to all unvisited directional paths that start at that endpoint.
  std::multimap<point_t, size_t> all_start;
  // A map from each endpoint to all unvisited directional paths that end at that endpoint.
  std::multimap<point_t, size_t> all_end;
  // A map from each endpoint to all unvisited bidi paths that start or end at that endpoint.
  std::multimap<point_t, size_t> all_bidi;
  // A map from each endpoint to the count of how many unvisited paths start or end at that endpoint.
  std::unordered_set<point_t> all_endpoints;
};

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
 
    std::vector<std::pair<std::list<point_t>, bool>> euler_paths;
    for (const auto& vertex : paths.get_all_endpoints()) {
      while (must_start(vertex)) {
        std::pair<std::list<point_t>, bool> new_path;
        new_path.first.push_back(vertex);
        new_path.second = true;
        while (append_one_path(new_path, std::prev(new_path.first.cend()))) {
          // keep going
        }
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
      stitch_loops(euler_path);
    }

    // Anything remaining is loops on islands.  Make all those paths, too.
    for (const auto& vertex : paths.get_all_endpoints()) {
      if (paths.count_unvisited_outgoing_paths(vertex) == 0 &&
          paths.count_unvisited_incoming_paths(vertex) == 0 &&
          paths.count_unvisited_bidi_paths(vertex) == 0) {
        // No unvisited paths connected to this vertex.
        continue;
      }
      std::pair<std::list<point_t>, bool> new_path;
      new_path.first.push_back(vertex);
      new_path.second = true;
      while (append_one_path(new_path, std::prev(new_path.first.cend()))) {
        // keep going
      }
      euler_paths.push_back(new_path);
      // We can stitch right now because all vertices already have even number of edges.
      stitch_loops(euler_paths.back());
    }

    std::vector<std::pair<linestring_t, bool>> ret;
    for (const auto& euler_path : euler_paths) {
      ret.push_back(std::make_pair(linestring_t(euler_path.first.cbegin(), euler_path.first.cend()), euler_path.second));
    }

    return ret;
  }

 private:
  bool must_start(const point_t& vertex) const {
    // A vertex must be a starting point if there are more out edges than in
    // edges, even after using the bidi edges.
    auto out_edges = paths.count_unvisited_outgoing_paths(vertex);
    auto in_edges = paths.count_unvisited_incoming_paths(vertex);
    auto bidi_edges = paths.count_unvisited_bidi_paths(vertex);
    return must_start_helper(out_edges, in_edges, bidi_edges);
  }

  // Higher score is better.
  template <typename p_t>
  double path_score(const typename std::list<point_t>& path_so_far,
                    const typename std::list<point_t>::const_iterator& where_to_add,
                    const linestring_t& current_path,
                    const Side& side,
                    identity<p_t>) {
    if (path_so_far.cbegin() == where_to_add || current_path.size() < 2) {
      // Doesn't matter, pick any.
      return 0;
    }
    auto p0 = *std::prev(where_to_add);
    auto p1 = *where_to_add;
    auto p2 = current_path[side == Side::front ? 1 : current_path.size()-2];

    // cos(theta) = (a dot b)/(|a|*|b|)
    // We don't need to take the cosine because it is decreasing over
    // the range of theta that we care about, so they are comparable.
    auto delta_x10 = p0.x() - p1.x();
    auto delta_y10 = p0.y() - p1.y();
    auto delta_x12 = p2.x() - p1.x();
    auto delta_y12 = p2.y() - p1.y();
    auto length_product = sqrt((delta_x10*delta_x10 + delta_y10*delta_y10) * (delta_x12*delta_x12 + delta_y12*delta_y12));
    auto dot_product = (delta_x10*delta_x12) + (delta_y10*delta_y12);
    return -dot_product/length_product;
  }

  double path_score(const typename std::list<point_t>&,
                    const typename std::list<point_t>::const_iterator&,
                    const linestring_t&,
                    const Side&,
                    identity<int>) {
    return 0;
  }

  template <typename p_t>
  double path_score(const typename std::list<point_t>& path_so_far,
                    const typename std::list<point_t>::const_iterator& where_to_add,
                    const linestring_t& current_path,
                    const Side& side) {
    return path_score(path_so_far, where_to_add, current_path, side, identity<p_t>());
  }

  // Pick the best path to insert into the path_so_far at where_to_add continue on given the path_so_far and a
  // range of options.  The range must have at least one element in it.  Also return the direction of that best path.
  std::pair<size_t, Side> select_path(
      const typename std::list<point_t>& path_so_far,
      const typename std::list<point_t>::const_iterator& where_to_add,
      const std::pair<typename std::multimap<point_t, size_t>::const_iterator,
                      typename std::multimap<point_t, size_t>::const_iterator>& options) {
    std::pair<size_t, Side> best; // best path to add and the direction to add it in.
    double best_score = std::numeric_limits<double>::lowest();
    for (auto current = options.first; current != options.second; current++) {
      auto current_path_index = current->second;
      auto current_path = paths.get_path(current_path_index).first;
      auto current_path_start = current_path.front();
      auto current_path_end = current_path.back();
      if (current_path_start == *where_to_add) {
        auto current_score = path_score<point_t>(path_so_far, where_to_add, current_path, Side::front);
        if (current_score > best_score) {
          best = std::make_pair(current_path_index, Side::front);
          best_score = current_score;
        }
      }
      if (current_path_end == *where_to_add) {
        auto current_score = path_score<point_t>(path_so_far, where_to_add, current_path, Side::back);
        if (current_score > best_score) {
          best = std::make_pair(current_path_index, Side::back);
          best_score = current_score;
        }
      }
    }
    return best;
  }

  // Given a path so far and where we want to add more to it, insert another path.
  // Return true if a path was inserted.
  bool append_one_path(std::pair<std::list<point_t>, bool>& path_so_far,
                       const typename std::list<point_t>::const_iterator& where_to_add) {
    // Find an unvisited path that leads from element before where_to_add.  Prefer out edges to bidi
    // because we may need to save the bidi edges to later be in edges.
    auto vertex_and_path_range = paths.get_unvisited_outgoing_paths(*where_to_add);
    if (vertex_and_path_range.first == vertex_and_path_range.second) {
      vertex_and_path_range = paths.get_unvisited_bidi_paths(*where_to_add);
      if (vertex_and_path_range.first == vertex_and_path_range.second) {
        return false; // Nothing inserted.
      }
    }
    auto path_to_add = select_path(path_so_far.first, where_to_add, vertex_and_path_range);
    insert_path(path_so_far, where_to_add, path_to_add);
    // After we've inserted the path, remove it from the path manager.
    paths.remove_path(path_to_add.first);
    return true;
  }

  void insert_path(std::pair<std::list<point_t>, bool>& path_so_far,
                   const typename std::list<point_t>::const_iterator& where_to_add,
                   std::pair<size_t, Side> path_to_add) {
    const auto path_index = path_to_add.first;
    const auto side = path_to_add.second;
    const auto& current_path = paths.get_path(path_index).first;
    const auto& reversible = paths.get_path(path_index).second;
    if (side == Side::front) {
      // Don't insert the first element because it is already the element before where_to_add.
      path_so_far.first.insert(std::next(where_to_add), current_path.cbegin()+1, current_path.cend());
    } else {
      // Don't insert the first element of the reverse list because it is already the element before where_to_add.
      path_so_far.first.insert(std::next(where_to_add), current_path.crbegin()+1, current_path.crend());
    }
    path_so_far.second &= reversible;
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
  void stitch_loops(std::pair<std::list<point_t>, bool>& euler_path) {
    auto& path = euler_path.first;

    for (auto it = path.cbegin(); it != path.cend(); it++) {
      // Make a path from here.  We don't need the first element, it's already in our path.
      append_one_path(euler_path, it);
    }
  }
  path_manager<point_t, linestring_t> paths;
}; //class eulerian_paths

// Returns a minimal number of toolpaths that include all the milling in the
// oroginal toolpaths.  Each path is traversed once.  Each path has a bool
// indicating if the path is reversible.
template <typename point_t, typename linestring_t>
std::vector<std::pair<linestring_t, bool>> get_eulerian_paths(const std::vector<std::pair<linestring_t, bool>>& paths) {
  return eulerian_paths<point_t, linestring_t>(
      paths).get();
}

multi_linestring_type_fp make_eulerian_paths(const multi_linestring_type_fp& paths, bool reversible, bool unique);

} // namespace eulerian_paths
#endif //EULERIAN_PATHS_H
