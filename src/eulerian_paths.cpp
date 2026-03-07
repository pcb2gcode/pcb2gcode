#include "geometry_int.hpp"
#include "bg_operators.hpp"
#include <vector>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <cmath>

#include "segmentize.hpp"
#include "eulerian_paths.hpp"
#include "options.hpp"

namespace eulerian_paths {

using std::vector;
using std::pair;

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

static inline Side operator^(const Side& s, bool flip) {
  if (flip) {
    return !s;
  } else {
    return s;
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

template <typename point_t, typename linestring_t>
class path_manager;

class path_and_direction {
 public:
  template <typename point_t, typename linestring_t> friend class path_manager;
  friend struct std::hash<path_and_direction>;
  path_and_direction() = default;
  path_and_direction(size_t path_index, Side side) : path_index(path_index), side(side) {}
  path_and_direction operator!() const {
    return path_and_direction(path_index, !side);
  }
  bool operator==(const path_and_direction& other) const {
    return path_index == other.path_index && side == other.side;
  }
  bool operator<(const path_and_direction& other) const {
    return std::tie(path_index, side) < std::tie(other.path_index, other.side);
  }
  friend std::ostream& operator<<(std::ostream& out, const path_and_direction& p) {
    out << "path_index: " << p.path_index << ", side: " << p.side;
    return out;
  }

 private:
  size_t path_index;
  Side side;
};

} // namespace eulerian_paths

// Hash specialization for path_and_direction
namespace std {
template <>
struct hash<eulerian_paths::path_and_direction> {
  size_t operator()(const eulerian_paths::path_and_direction& p) const noexcept {
    return hash<std::pair<size_t, eulerian_paths::Side>>()(std::make_pair(p.path_index, p.side));
  }
};
} // namespace std

namespace eulerian_paths {

template <typename linestring_t>
class linestring_iterator {
 public:
  using value_type = typename linestring_t::value_type;
  using difference_type = typename linestring_t::difference_type;
  using pointer = typename linestring_t::const_pointer;
  using reference = typename linestring_t::const_reference;
  using iterator_category = std::bidirectional_iterator_tag;

  linestring_iterator() = default;
  linestring_iterator(typename linestring_t::const_iterator it) : it(it) {}
  linestring_iterator(typename linestring_t::const_reverse_iterator it) : it(it) {}

  reference operator*() const {
    return boost::apply_visitor([](auto&& iter) -> reference { return *iter; }, it);
  }

  linestring_iterator& operator++() {
    boost::apply_visitor([](auto& iter) { ++iter; }, it);
    return *this;
  }

  linestring_iterator& operator--() {
    boost::apply_visitor([](auto& iter) { --iter; }, it);
    return *this;
  }

  linestring_iterator operator+(difference_type n) const {
    return boost::apply_visitor([n](auto iter) {
      return linestring_iterator(iter + n);
    }, it);
  }

  bool operator!=(const linestring_iterator& other) const {
    return it != other.it;
  }

 private:
  boost::variant<typename linestring_t::const_iterator, typename linestring_t::const_reverse_iterator> it;
};

template <typename point_t>
double get_cosine_of_angle_impl(const point_t& p0, const point_t& p1, const point_t& p2) {
  auto delta_x10 = p0.x() - p1.x();
  auto delta_y10 = p0.y() - p1.y();
  auto delta_x12 = p2.x() - p1.x();
  auto delta_y12 = p2.y() - p1.y();
  auto length_product = std::sqrt((delta_x10*delta_x10 + delta_y10*delta_y10)) * std::sqrt((delta_x12*delta_x12 + delta_y12*delta_y12));
  auto dot_product = (delta_x10*delta_x12) + (delta_y10*delta_y12);
  return dot_product/length_product;
}

double get_cosine_of_angle_impl(const int& p0, const int& p1, const int& p2) {
  double ret;
  if (p0 == p1 || p1 == p2) {
    ret = 0;  // Undefined.
  } else if (p0 < p1 && p1 < p2) {
    ret = -1; // Straight line.
  } else if (p0 > p1 && p1 > p2) {
    ret = -1; // Straight line.
  } else {
    ret = 1; // Angle 0
  }
  return ret;
}

template <typename point_t>
double get_cosine_of_angle(const point_t& p0, const point_t& p1, const point_t& p2) {
  return get_cosine_of_angle_impl(p0, p1, p2);
}

/* This class holds on to all the paths and uses std::multimap internally to
 * make it quick to look up which paths extend from a given vertex and in which
 * direction. */
template <typename point_t, typename linestring_t>
class path_manager {
 public:
  // The bool indicates if the path is reversible.
  path_manager(const std::vector<std::pair<linestring_t, bool>>& paths) : paths(paths) {
    for (size_t index = 0; index < paths.size(); index++) {
      auto const& path = paths[index].first;
      if (path.size() < 2) {
        // Valid path must have a start and end.
        continue;
      }
      point_t start = path.front();
      point_t end = path.back();
      all_vertices.insert(start);
      all_vertices.insert(end);
      auto const reversible = paths[index].second;
      if (reversible) {
        bidi_vertex_to_unvisited_path_index.emplace(start, path_and_direction(index, Side::front));
        bidi_vertex_to_unvisited_path_index.emplace(end, path_and_direction(index, Side::back));
      } else {
        start_vertex_to_unvisited_path_index.emplace(start, path_and_direction(index, Side::front));
        end_vertex_to_unvisited_path_index.emplace(end, path_and_direction(index, Side::back));
      }
    }
    for (auto const& bidi_edge : bidi_vertex_to_unvisited_path_index) {
      auto const score = compute_bidi_conversion_score(bidi_edge.second);
      bidi_conversion_score_cache.emplace(bidi_edge.second, score);
      bidi_conversion_sorted_scores.emplace(score, bidi_edge.second);
    }
  }

  void bidi_to_directional(path_and_direction index_and_side, bool flip) {
    // Remove it from the bidi_vertex_to_unvisited_path_index.
    remove_path(index_and_side);
    // We won't change the original reversability aspect, just make it directional.
    // Add it to the directional maps, in the correct direction.
    index_and_side.side = index_and_side.side ^ flip;
    auto const start_vertex = get_front(index_and_side);
    auto const end_vertex = get_back(index_and_side);
    start_vertex_to_unvisited_path_index.emplace(start_vertex, path_and_direction(index_and_side.path_index, index_and_side.side));
    end_vertex_to_unvisited_path_index.emplace(end_vertex, path_and_direction(index_and_side.path_index, !index_and_side.side));
    // Converting an edge from bidi to directional invalidates cached scores for all bidi edges
    // that start or end at the endpoints of the converted edge.
    auto start_range = bidi_vertex_to_unvisited_path_index.equal_range(start_vertex);
    for (auto it = start_range.first; it != start_range.second; ++it) {
      auto const old_score = bidi_conversion_score_cache.at(it->second);
      auto const new_score = compute_bidi_conversion_score(it->second);
      if (old_score != new_score) {
        bidi_conversion_score_cache[it->second] = new_score; // Replace the old score with the new one.
        bidi_conversion_sorted_scores.erase({old_score, it->second}); // Remove the old score from the sorted scores.
        bidi_conversion_sorted_scores.emplace(new_score, it->second); // Add the new score to the sorted scores.
      }
    }
    auto end_range = bidi_vertex_to_unvisited_path_index.equal_range(end_vertex);
    for (auto it = end_range.first; it != end_range.second; ++it) {
      auto const old_score = bidi_conversion_score_cache.at(it->second);
      auto const new_score = compute_bidi_conversion_score(it->second);
      if (old_score != new_score) {
        bidi_conversion_score_cache[it->second] = new_score; // Replace the old score with the new one.
        bidi_conversion_sorted_scores.erase({old_score, it->second}); // Remove the old score from the sorted scores.
        bidi_conversion_sorted_scores.emplace(new_score, it->second); // Add the new score to the sorted scores.
      }
    }
  }
  auto& get_all_vertices() const {
    return all_vertices;
  }
  auto& get_start_vertex_to_unvisited_path_index() const {
    return start_vertex_to_unvisited_path_index;
  }
  auto& get_bidi_vertex_to_unvisited_path_index() const {
    return bidi_vertex_to_unvisited_path_index;
  }
  auto& get_end_vertex_to_unvisited_path_index() const {
    return end_vertex_to_unvisited_path_index;
  }
  auto& get_path(size_t index) const {
    return paths[index];
  }
  void remove_path(path_and_direction const index_and_side) {
    auto& path = paths[index_and_side.path_index].first;
    // We don't know if the path is being used as a bidi path or a directional path and
    // we don't know if it is used in a forward or reverse direction.  Just remove it from all
    // three maps.
    for (auto& map : {&start_vertex_to_unvisited_path_index,
                      &end_vertex_to_unvisited_path_index,
                      &bidi_vertex_to_unvisited_path_index}) {
      for (auto& vertex : {path.front(), path.back()}) {
        auto vertex_range = map->equal_range(vertex);
        for (auto vertex_it = vertex_range.first; vertex_it != vertex_range.second; vertex_it++) {
          if (vertex_it->second.path_index == index_and_side.path_index) {
            path_and_direction const to_erase = vertex_it->second; // Save value before iterator invalidation.
            map->erase(vertex_it);
            // If it happens to be a bidi edge, remove it from the cache.
            if (map == &bidi_vertex_to_unvisited_path_index) {
              auto const score = bidi_conversion_score_cache.at(to_erase);
              bidi_conversion_score_cache.erase(to_erase);
              bidi_conversion_sorted_scores.erase({score, to_erase});
            }
            break;
          }
        }
      }
    }
  }
  auto get_point(path_and_direction const& index_and_side, int index) const {
    if (index >= 0) {
      return *(get_cbegin(index_and_side) + index);
    } else {
      return *(get_cend(index_and_side) + index);
    }
  }
  auto get_front(path_and_direction const& index_and_side) const {
    return get_point(index_and_side, 0);
  }
  auto get_back(path_and_direction const& index_and_side) const {
    return get_point(index_and_side, -1);
  }
  auto get_path_reversible(path_and_direction const& index_and_side) const {
    return paths[index_and_side.path_index].second;
  }
  auto get_path_size(path_and_direction const& index_and_side) const {
    return paths[index_and_side.path_index].first.size();
  }
  auto get_cbegin(path_and_direction const& index_and_side) const {
    linestring_iterator<linestring_t> ret;
    if (index_and_side.side == Side::front) {
      ret = paths[index_and_side.path_index].first.cbegin();
    } else {
      ret = paths[index_and_side.path_index].first.crbegin();
    }
    return ret;
  }
  auto get_cend(path_and_direction const& index_and_side) const {
    linestring_iterator<linestring_t> ret;
    if (index_and_side.side == Side::front) {
      ret = paths[index_and_side.path_index].first.cend();
    } else {
      ret = paths[index_and_side.path_index].first.crend();
    }
    return ret;
  }

  // Returns the bidi edge with the lowest conversion score. Call only when there is at least one bidi edge.
  path_and_direction choose_best_bidi_edge_to_convert() const {
    return bidi_conversion_sorted_scores.begin()->second;
  }

private:
  // Score for choosing which bidi edge to convert to directional next. Lower is better.
  std::pair<int, double> compute_bidi_conversion_score(path_and_direction bidi_edge) const {
    auto const out_edges_at_end = start_vertex_to_unvisited_path_index.count(get_back(bidi_edge));
    auto const in_edges_at_end = end_vertex_to_unvisited_path_index.count(get_back(bidi_edge));
    auto const in_edges_at_start = end_vertex_to_unvisited_path_index.count(get_front(bidi_edge));
    auto const out_edges_at_start = start_vertex_to_unvisited_path_index.count(get_front(bidi_edge));
    auto const imbalance = (in_edges_at_end < out_edges_at_end ? 0 : 1) +
                           (out_edges_at_start < in_edges_at_start ? 0 : 1);
    double best_start_cosine = 1;
    auto start_range = start_vertex_to_unvisited_path_index.equal_range(get_back(bidi_edge));
    for (auto it = start_range.first; it != start_range.second; ++it) {
      auto const& option_edge = it->second;
      auto const& p0 = get_point(bidi_edge, -2);
      auto const& p1 = get_point(option_edge, 0);
      auto const& p2 = get_point(option_edge, 1);
      auto const cosine_of_angle = get_cosine_of_angle(p0, p1, p2);
      if (cosine_of_angle < best_start_cosine) {
        best_start_cosine = cosine_of_angle;
      }
    }
    double best_end_cosine = 1;
    auto end_range = end_vertex_to_unvisited_path_index.equal_range(get_front(bidi_edge));
    for (auto it = end_range.first; it != end_range.second; ++it) {
      auto const& option_edge = it->second;
      auto const& p0 = get_point(option_edge, 1);
      auto const& p1 = get_point(option_edge, 0);
      auto const& p2 = get_point(bidi_edge, 1);
      auto const cosine_of_angle = get_cosine_of_angle(p0, p1, p2);
      if (cosine_of_angle < best_end_cosine) {
        best_end_cosine = cosine_of_angle;
      }
    }
    auto const result = std::make_pair(imbalance, best_start_cosine + best_end_cosine);
    return result;
  }

private:
  std::vector<std::pair<linestring_t, bool>> const& paths;
  // Create a map from vertex to each path that start at that vertex.
  // It's a map to an index into the input paths.  The bool tells us
  // if the point_t is at the front or back.  For start, it will
  // always be true.
  std::multimap<point_t, path_and_direction> start_vertex_to_unvisited_path_index;
  // Create a map from vertex to each bidi path that may start or end
  // at that vertex.  It's a map to an index into the input paths.
  // The bool tells us if the point_t is at the front or back.  For
  // bidi, it could be either.
  std::multimap<point_t, path_and_direction> bidi_vertex_to_unvisited_path_index;
  // Create a map from vertex to each path that may start or end at
  // that vertex.  It's a map to an index into the input paths.  The
  // bool tells us if the point_t is at the front or back.  For end,
  // it will always be false.
  std::multimap<point_t, path_and_direction> end_vertex_to_unvisited_path_index;
  // Only the ones that have at least one potential edge leading out.
  std::set<point_t> all_vertices;
  mutable std::unordered_map<path_and_direction, std::pair<int, double>> bidi_conversion_score_cache;
  mutable std::set<std::pair<std::pair<int, double>, path_and_direction>> bidi_conversion_sorted_scores;
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
    /* We use Hierholzer's algorithm to find the minimum cycles.  However, some
     * paths are bidirectional and some are not.  To deal with this, we first convert
     * all paths to directional.  Then we use Hierholzer's algorithm to find the
     * minimum cycles.
     */
    // Convert all paths to bidirectional paths.
    auto vertices_to_examine = paths.get_all_vertices();
    while (paths.get_bidi_vertex_to_unvisited_path_index().size() > 0) {
      while (vertices_to_examine.size() > 0) {
        auto const vertex = *vertices_to_examine.begin();
        auto const out_edges = paths.get_start_vertex_to_unvisited_path_index().count(vertex);
        auto const in_edges = paths.get_end_vertex_to_unvisited_path_index().count(vertex);
        std::unordered_set<path_and_direction> seen_bidi_edges;
        auto bidi_edge_range = paths.get_bidi_vertex_to_unvisited_path_index().equal_range(vertex);
        for (auto bidi_edge_ptr = bidi_edge_range.first; bidi_edge_ptr != bidi_edge_range.second; bidi_edge_ptr++) {
          // Any bidi edge for which is connected to this vertex in both directions must be ignored.
          path_and_direction bidi_edge = bidi_edge_ptr->second;
          if (seen_bidi_edges.count(!bidi_edge) == 0) {
            seen_bidi_edges.insert(bidi_edge);
          } else {
            // If we have already seen this bidi edge in the other direction,
            // ignore it because we know that this is a self-loop.
            // Also, we have to account for ignoring the first time that we saw it.
            seen_bidi_edges.erase(!bidi_edge);
          }
        }
        auto const bidi_edges = seen_bidi_edges.size();
        bool convert_to_out = in_edges >= bidi_edges + out_edges;
        bool convert_to_in = out_edges >= bidi_edges + in_edges;
        if (bidi_edges && (convert_to_out ^ convert_to_in)) {
          // If either convert_to_out or convert_to_in, we can convert bidi edges.
          // If neither are true, we can't convert.
          // If both are true, there are no bidi edges to convert.
          for (const auto& bidi_edge_and_path : seen_bidi_edges) {
            // We need to flip if the edge is connected in the wrong direction.
            bool flip = convert_to_in;
            paths.bidi_to_directional(bidi_edge_and_path, flip);
            // The endpoints of the edge need to be re-examined.
            vertices_to_examine.insert(paths.get_front(bidi_edge_and_path));
            vertices_to_examine.insert(paths.get_back(bidi_edge_and_path));
          }
        }
        // Remove the vertex from the set of vertices to examine.
        vertices_to_examine.erase(vertex);
      }
      if (paths.get_bidi_vertex_to_unvisited_path_index().size() == 0) {
        // We've examined all vertices and converted as many bidi edges to
        // directional edges as possible and there are no bidi edges left.
        break;
      }
      // There are still some bidi edges left. Pick the one with the best conversion score.
      path_and_direction best_path_index_and_side = paths.choose_best_bidi_edge_to_convert();
      paths.bidi_to_directional(best_path_index_and_side, false);
      // The endpoints of the bidi edge need to be re-examined.
      vertices_to_examine.insert(paths.get_front(best_path_index_and_side));
      vertices_to_examine.insert(paths.get_back(best_path_index_and_side));
    }

    // All edges are now directional.
    std::vector<std::pair<linestring_t, bool>> euler_paths;
    for (const auto& vertex : paths.get_all_vertices()) {
      while (must_start(vertex)) {
        // Make a path starting from vertex with odd count.
        std::pair<linestring_t, bool> new_path({vertex}, true);
        while (insert_one_path(&new_path, new_path.first.size()-1) > 0) {
          // Keep going.
        }
        euler_paths.push_back(new_path);
      }
    }
    // All vertices have the number of out edges less than or equal to the number of in edges.
    // We know that the total number of out edges is equal to the total number of in edges.
    // If there existed a vertex with more in edges than out edges, that would mean that all the other
    // vertices would together have more out edges than in edges, in total.  But that would mean that there must
    // be some other vertex with more out edges than in edges, which we already said is not true.  So it
    // must be the case that all vertices have the same number of out edges as in edges.  Which means that only
    // loops remain.  So now we can stitch.
    for (auto& euler_path : euler_paths) {
      stitch_loops(&euler_path);
    }

    // Anything remaining is loops on islands.  Make all those paths, too.
    for (const auto& vertex : paths.get_all_vertices()) {
      std::pair<linestring_t, bool> new_path({vertex}, true);
      while (insert_one_path(&new_path, new_path.first.size()-1) > 0) {
        // Keep going.
      }
      if (new_path.first.size() > 1) {
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
    auto out_edges = paths.get_start_vertex_to_unvisited_path_index().count(vertex);
    auto in_edges = paths.get_end_vertex_to_unvisited_path_index().count(vertex);
    return out_edges > in_edges;
  }

  // Higher score is better.
  template <typename p_t>
  double path_score(const linestring_t& path_so_far,
                    const size_t where_to_start,
                    const std::pair<point_t, path_and_direction>& option,
                    identity<p_t>) {
    if (where_to_start < 1 || paths.get_path_size(option.second) < 2) {
      // Doesn't matter, pick any.
      return 0;
    }
    auto p0 = path_so_far[where_to_start-1];
    auto p1 = path_so_far[where_to_start];
    // If the back of the path is what is connected to this vertex then we want to attach in reverse.
    auto p2 = paths.get_point(option.second, 1);

    // cos(theta) = (a dot b)/(|a|*|b|)
    // We don't need to take the cosine because it is decreasing over
    // the range of theta that we care about, so they are comparable.
    // Lowest is best.
    return get_cosine_of_angle(p0, p1, p2);
  }

  double path_score(const linestring_t&,
                    const size_t,
                    const std::pair<point_t, path_and_direction>&,
                    identity<int>) {
    return 0;
  }

  template <typename p_t>
  double path_score(const linestring_t& path_so_far,
                    const size_t where_to_start,
                    const std::pair<point_t, path_and_direction>& option) {
    return path_score(path_so_far, where_to_start, option, identity<p_t>());
  }

  // Pick the best path to continue on given the path_so_far and a
  // range of options.  The range must have at least one element in
  // it.
  typename std::multimap<point_t, path_and_direction>::const_iterator select_path(
      const linestring_t& path_so_far,
      const size_t where_to_start,
      const std::pair<typename std::multimap<point_t, path_and_direction>::const_iterator,
                      typename std::multimap<point_t, path_and_direction>::const_iterator>& options) {
    auto best = options.first;
    double best_score = path_score<point_t>(path_so_far, where_to_start, *best);
    for (auto current = options.first; current != options.second; current++) {
      double current_score = path_score<point_t>(path_so_far, where_to_start, *current);
      if (current_score < best_score) {
        best = current;
        best_score = current_score;
      }
    }
    return best;
  }

  // Given a point, make a path from that point as long as possible
  // until a dead end.  Assume that point itself is already in the
  // list.  Return the number of elements inserted.  Only call this
  // when there are no bidi edges.
  size_t insert_one_path(std::pair<linestring_t, bool>* new_path, const size_t where_to_start) {
    // Find an unvisited path that leads from point.  Prefer out edges to bidi
    // because we may need to save the bidi edges to later be in edges.
    auto vertex_and_path_range = paths.get_start_vertex_to_unvisited_path_index().equal_range(new_path->first.at(where_to_start));
    if (vertex_and_path_range.first == vertex_and_path_range.second) {
      // No more paths to follow.
      return 0;
    }
    auto vertex_and_path_index = *select_path(new_path->first, where_to_start, vertex_and_path_range);
    new_path->first.insert(new_path->first.begin() + where_to_start + 1, paths.get_cbegin(vertex_and_path_index.second) + 1, paths.get_cend(vertex_and_path_index.second));
    paths.remove_path(vertex_and_path_index.second);
    // Update the reversability of the new path.
    new_path->second = new_path->second && paths.get_path_reversible(vertex_and_path_index.second);
    return paths.get_path_size(vertex_and_path_index.second) - 1;
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
    for (size_t i = 0; i < euler_path->first.size(); i++) {
      // Make a path from here, as long as possible.  Because we only have loops left in the
      // paths, the path will end where it started.
      auto insert_at = i;
      do {
        auto const insert_length = insert_one_path(euler_path, insert_at);
        if (insert_length == 0) {
          break;
        }
        // Keep going.  It's sure to end where we started because it is a loop.
        insert_at += insert_length;
      } while (true);
    }
  }
  path_manager<point_t, linestring_t> paths;
}; //class eulerian_paths

template <typename point_t, typename linestring_t>
bool check_eulerian_paths(const std::vector<std::pair<linestring_t, bool>>& before,
                          const std::vector<std::pair<linestring_t, bool>>& after) {
  std::unordered_multiset<std::pair<std::pair<point_t, point_t>, bool>> all_edges;
  // Cheeck that each edge in the input is also in the output.
  // Also check that the number of output paths is reasonable.
  // At worst, nothing connected and the number of lines and rings is unchanged.
  size_t num_lines_before = 0;
  size_t num_rings_before = 0;
  for (auto const& linestring : before) {
    auto const reversible = linestring.second;
    auto const path = linestring.first;
    if (path.size() < 2) {
      continue;
    }
    if (path.front() == path.back()) {
      num_rings_before++;
    } else {
      num_lines_before++;
    }
    for (size_t i = 0; i < path.size()-1; i++) {
      auto const p0 = path[i];
      auto const p1 = path[i+1];
      auto const edge = std::make_pair(p0, p1);
      all_edges.insert({edge, reversible});
    }
  }
  size_t num_lines_after = 0;
  size_t num_rings_after = 0;
  for (auto const& linestring : after) {
    auto const path = linestring.first;
    if (path.front() == path.back()) {
      num_rings_after++;
    } else {
      num_lines_after++;
    }
    for (size_t i = 0; i < path.size()-1; i++) {
      auto const p0 = path[i];
      auto const p1 = path[i+1];
      auto const edge = std::make_pair(p0, p1);
      auto const reversed_edge = std::make_pair(p1, p0);
      // If the edge is in all_edges twice, reversible and not, we prefer to delete the non-reversible one first because the other one can be more
      // flexible for searching later.
      if (all_edges.find({edge, false}) != all_edges.end()) {
        all_edges.erase(all_edges.find({edge, false}));
      } else if (all_edges.find({edge, true}) != all_edges.end()) {
        all_edges.erase(all_edges.find({edge, true}));
      } else if (all_edges.find({reversed_edge, true}) != all_edges.end()) {
        all_edges.erase(all_edges.find({reversed_edge, true}));
      } else {
        return false;
      }
    }
  }
  if (all_edges.size() > 0) {
    return false;
  }
  auto const num_elements_after = num_lines_after + num_rings_after;
  auto const num_elements_before = num_lines_before + num_rings_before;
  // We expect to always do at least as well as the input.  Also, the only way to have more rings than before is to combine
  // lines.  So if we have fewer rings in the result, we should have fewer elements over all.
  if (num_elements_after > num_elements_before || // We should not see the number of paths increase.
       // If we didn't decrease the number of elements then we should at least not see fewer rings.
      (num_elements_after == num_elements_before && num_rings_after < num_rings_before)) {
    return false;
  }
  return true;
}

template
bool check_eulerian_paths<point_type_fp, linestring_type_fp>(
    const std::vector<std::pair<linestring_type_fp, bool>>& before,
    const std::vector<std::pair<linestring_type_fp, bool>>& after);

template
bool check_eulerian_paths<point_type, linestring_type>(
    const std::vector<std::pair<linestring_type, bool>>& before,
    const std::vector<std::pair<linestring_type, bool>>& after);

template
bool check_eulerian_paths<int, std::vector<int>>(
    const std::vector<std::pair<std::vector<int>, bool>>& before,
    const std::vector<std::pair<std::vector<int>, bool>>& after);

// Returns a minimal number of toolpaths that include all the milling in the
// oroginal toolpaths.  Each path is traversed once.  Each path has a bool
// indicating if the path is reversible.
template <typename point_t, typename linestring_t>
std::vector<std::pair<linestring_t, bool>> get_eulerian_paths(const std::vector<std::pair<linestring_t, bool>>& paths) {
  auto const ret = eulerian_paths<point_t, linestring_t>(
      paths).get();
  if (options::get_vm().count("sanity-checks")) {
    if (!check_eulerian_paths<point_t, linestring_t>(paths, ret)) {
      options::maybe_throw("Sanity checks failed", ERR_SANITY_CHECKS_FAILED);
    }
  }
  return ret;
}

template
std::vector<std::pair<std::vector<int>, bool>> get_eulerian_paths<int, std::vector<int>>(
    const std::vector<std::pair<std::vector<int>, bool>>& paths);

template
std::vector<std::pair<linestring_type_fp, bool>> get_eulerian_paths<point_type_fp, linestring_type_fp>(
    const std::vector<std::pair<linestring_type_fp, bool>>& paths);

template
std::vector<std::pair<linestring_type, bool>> get_eulerian_paths<point_type, linestring_type>(
    const std::vector<std::pair<linestring_type, bool>>& paths);

// This calls segmentize and then get_eulerian_paths.  If unique is
// true, remove repeated segments.
multi_linestring_type_fp make_eulerian_paths(const multi_linestring_type_fp& paths, bool reversible, bool unique) {
  vector<pair<linestring_type_fp, bool>> path_to_simplify;
  for (const auto& ls : paths) {
    path_to_simplify.push_back(std::make_pair(ls, reversible));
  }
  path_to_simplify = segmentize::segmentize_paths(path_to_simplify);
  if (unique) {
    path_to_simplify = segmentize::unique(path_to_simplify);
  }
  auto eulerian_paths = get_eulerian_paths<
      point_type_fp,
      linestring_type_fp>(path_to_simplify);
  multi_linestring_type_fp ret;
  for (auto& eulerian_path : eulerian_paths) {
    ret.push_back(eulerian_path.first);
  }
  return ret;
}

} // namespace eulerian_paths
