#include <vector>
using std::vector;

#include <unordered_map>
using std::unordered_map;

#include <unordered_set>
using std::unordered_set;

#include <queue>
using std::priority_queue;

#include <utility>
using std::pair;
using std::make_pair;

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/astar_search.hpp>
#include <boost/optional.hpp>

#include "path_finding.hpp"
#include "options.hpp"
#include "geometry.hpp"
#include "bg_operators.hpp"
#include "bg_helpers.hpp"

namespace path_finding {

using boost::optional;
using boost::make_optional;

Neighbors::iterator Neighbors::iterator::operator++() {
  do {
    if (ring_index < 2) {
      ring_index++;
    } else {
      point_index++;
      while (ring_index-2 < neighbors->pfs->vertices().size() &&
             point_index >= neighbors->pfs->vertices()[ring_index-2].size()) {
        ring_index++;
        point_index = 0;
      }
    }
  } while (ring_index < neighbors->pfs->vertices().size()+2 &&
           !neighbors->is_neighbor(**this));
  return *this;
}

bool Neighbors::iterator::operator!=(const Neighbors::iterator& other) const {
  return ring_index != other.ring_index || point_index != other.point_index;
}

bool Neighbors::iterator::operator==(const Neighbors::iterator& other) const {
  return !(*this != other);
}

const point_type_fp& Neighbors::iterator::operator*() const {
  if (ring_index == 0) {
    return neighbors->start;
  } else if (ring_index == 1) {
    return neighbors->goal;
  } else {
    return neighbors->pfs->vertices()[ring_index-2][point_index];
  }
}

Neighbors::Neighbors(const point_type_fp& start, const point_type_fp& goal,
                     const point_type_fp& current,
                     const RingIndices& ring_indices,
                     coordinate_type_fp g_score_current,
                     const PathLimiter path_limiter,
                     const PathFindingSurface* pfs) :
    start(start),
    goal(goal),
    current(current),
    ring_indices(ring_indices),
    g_score_current(g_score_current),
    path_limiter(path_limiter),
    pfs(pfs) {}

// Returns a valid neighbor index that is either the one provided or
// the next higher valid one.
bool Neighbors::is_neighbor(const point_type_fp p) const {
  if (p == current) {
    return false;
  }
  if (path_limiter != nullptr && path_limiter(p, g_score_current + bg::distance(current, p))) {
    return false;
  }
  if (!pfs->in_surface(current, p, ring_indices)) {
    return false;
  }
  return true;
}

Neighbors::iterator Neighbors::begin() const {
  auto ret = iterator(this, 0, 0);
  if (ret == end()) {
    // Can't dereferfence the end.
    return ret;
  }
  if (is_neighbor(*ret)) {
    // This is a valid begin.
    return ret;
  } else {
    // This position is invalid but we can increment to get to a valid one.
    ++ret;
    return ret;
  }
}

Neighbors::iterator Neighbors::end() const {
  return iterator(this, 2+pfs->vertices().size(), 0);
}

PathFindingSurface::PathFindingSurface(const optional<multi_polygon_type_fp>& keep_in,
                                       const multi_polygon_type_fp& keep_out,
                                       const coordinate_type_fp tolerance) {
  if (!keep_in && keep_out.size() == 0) {
    return;
  }
  if (keep_in) {
    multi_polygon_type_fp total_keep_in = *keep_in - keep_out;

    total_keep_in_grown.emplace();
    for (const auto& poly : total_keep_in) {
      all_vertices.emplace_back(poly.outer().cbegin(), poly.outer().cend());
      total_keep_in_grown->emplace_back(
          bg_helpers::buffer_miter(poly.outer(), tolerance),
          vector<multi_polygon_type_fp>());
      for (const auto& inner : poly.inners()) {
        all_vertices.emplace_back(inner.cbegin(), inner.cend());
        // Because the inner is reversed, we need to reverse it so
        // that the buffer algorithm won't get confused.
        auto temp_inner = inner;
        bg::reverse(temp_inner);
        // tolerance needs to be inverted because growing a shape
        // shrinks the holes in it.
        total_keep_in_grown->back().second.push_back(bg_helpers::buffer_miter(temp_inner, -tolerance));
      }
    }
  } else {
    for (const auto& poly : keep_out){
      all_vertices.emplace_back(poly.outer().cbegin(), poly.outer().cend());
      keep_out_shrunk.emplace_back(
          bg_helpers::buffer_miter(poly.outer(), -tolerance),
          vector<multi_polygon_type_fp>());
      for (const auto& inner : poly.inners()) {
        all_vertices.emplace_back(inner.cbegin(), inner.cend());
        // Because the inner is reversed, we need to reverse it so
        // that the buffer algorithm won't get confused.
        auto temp_inner = inner;
        bg::reverse(temp_inner);
        // tolerance needs to be inverted because shrinking a shape
        // grows the holes in it.
        keep_out_shrunk.back().second.push_back(bg_helpers::buffer_miter(temp_inner, tolerance));
      }
    }
  }

  for (auto av : all_vertices) {
    sort(av.begin(), av.end());
    av.erase(std::unique(av.begin(), av.end()), av.end());
  }
}

vector<size_t> inside_multipolygon(const point_type_fp& p,
                                   const multi_polygon_type_fp& mp) {
  size_t current_ring_index = 0;
  for (const auto& poly : mp) {
    if (point_in_ring(p, poly.outer())) {
      // Might be part of this shape but only if the point isn't in
      // the inners.
      vector<size_t> ring_indices{current_ring_index};
      current_ring_index++;
      for (const auto& inner : poly.inners()) {
        if (!point_in_ring(p, inner)) {
          // We'll have to make sure not to cross this inner.
          ring_indices.push_back(current_ring_index);
          current_ring_index++;
        } else {
          break; // We're inside one of the inners so give up.
        }
      }
      if (ring_indices.size() == poly.inners().size() + 1) {
        // We're inside this shape so we're done.
        return ring_indices;
      } else {
        // We're inside the outer but also inside an inner!  It might
        // be an outer in an inner so we'll ignore this one and keep
        // searching.  Reset the current_ring_index to what it was.
        current_ring_index = ring_indices.front();
        ring_indices.clear();
      }
    }
    current_ring_index += 1 + poly.inners().size();
  }
  return {};
}

vector<size_t> outside_multipolygon(const point_type_fp& p,
                                    const multi_polygon_type_fp& mp) {
  vector<size_t> ring_indices;
  size_t current_ring_index = 0;
  for (const auto& poly : mp) {
    if (point_in_ring(p, poly.outer())) {
      // We're inside the outer, maybe we're in an inner?  If not, we
      // aren't outside at all and we'll just give up.
      bool in_inner = false;
      for (size_t i = 0; i < poly.inners().size(); i++) {
        const auto& inner = poly.inners()[i];
        if (point_in_ring(p, inner)) {
          in_inner = true;
          ring_indices.push_back(current_ring_index + i + 1);
          break;
        }
      }
      if (!in_inner) {
        // We're inside the outer but not in any of the inners, so
        // we're in the shape, but we want to be outside the shape, so
        // we've failed.
        return {};
      }
      current_ring_index += 1 + poly.inners().size();
    } else {
      // We need to keep out of this outer.
      ring_indices.push_back(current_ring_index);
      // No need to examine the inners which we can't possibly be inside.
      current_ring_index += 1 + poly.inners().size();
    }
  }
  return ring_indices;
}

RingIndices inside_multipolygons(
    const point_type_fp& p,
    const nested_multipolygon_type_fp& mp){
  size_t current_ring_index = 0;
  for (const auto& poly : mp) {
    // The loop always starts on an outer.
    auto inside_mp = inside_multipolygon(p, poly.first);
    if (inside_mp.size() > 0) {
      // Might be part of this shape but only if the point isn't in
      // the inners.
      RingIndices ring_indices{{current_ring_index, inside_mp}};
      current_ring_index++;
      for (const auto& inner : poly.second) {
        auto outside_mp = outside_multipolygon(p, inner);
        if (outside_mp.size() > 0) {
          // We'll have to make sure not to cross this inner.
          ring_indices.push_back({current_ring_index, outside_mp});
          current_ring_index++;
        } else {
          break; // We're inside one of the inners so give up.
        }
      }
      if (ring_indices.size() == poly.second.size() + 1) {
        // We never hit the break so we're inside this shape so we're
        // done.
        return ring_indices;
      } else {
        // We're inside the outer but also inside an inner!  It might
        // be an outer in an inner so we'll ignore this one and keep
        // searching.  Reset the current_ring_index to what it was.
        current_ring_index = ring_indices.front().first;
        ring_indices.clear();
      }
    }
    current_ring_index += 1 + poly.second.size();
  }
  return {};
}

RingIndices outside_multipolygons(
    const point_type_fp& p,
    const nested_multipolygon_type_fp& mp) {
  RingIndices ring_indices;
  size_t current_ring_index = 0;
  for (const auto& poly : mp) {
    auto outside_mp = outside_multipolygon(p, poly.first);
    if (outside_mp.size() == 0) {
      // We're inside the outer, maybe we're in an inner?  If not, we
      // aren't outside at all and we'll just give up.
      bool in_inner = false;
      for (size_t i = 0; i < poly.second.size(); i++) {
        const auto& inner = poly.second[i];
        auto inside_mp = inside_multipolygon(p, inner);
        if (inside_mp.size() > 0) {
          in_inner = true;
          ring_indices.push_back({current_ring_index + i + 1, inside_mp});
          break;
        }
      }
      if (!in_inner) {
        // We're inside the outer but not in any of the inners, so
        // we're in the shape, but we want to be outside the shape, so
        // we've failed.
        return {};
      }
      current_ring_index += 1 + poly.second.size();
    } else {
      // We need to keep out of this outer.
      ring_indices.push_back({current_ring_index, outside_mp});
      // No need to examine the inners which we can't possibly be inside.
      current_ring_index += 1 + poly.second.size();
    }
  }
  return ring_indices;
}

/* Given a point, determine if the point is in the search surface.  If
   so, return a non-default value, otherwise default value.  If two
   points return the same value, there is a path between them in the
   surface.  If not then there cannot be a path between them.  The
   value will actually be a vector of size_t that indicates which
   rings in the stored polygon should be used for the generated points
   in the path and also for the collision detection. */
RingIndices PathFindingSurface::in_surface(point_type_fp p) const {
  if (total_keep_in_grown) {
    return inside_multipolygons(p, *total_keep_in_grown);
  } else {
    return outside_multipolygons(p, keep_out_shrunk);
  }
}

bool is_intersecting(const point_type_fp& a, const point_type_fp& b, const ring_type_fp& ring) {
  for (auto current = ring.cbegin(); current + 1 != ring.cend(); current++) {
    if (is_intersecting(a, b, *current, *(current+1))) {
      return true;
    }
  }
  return false;
}

bool is_intersecting(
    const point_type_fp& a, const point_type_fp& b,
    const vector<size_t>& ring_indices,
    const multi_polygon_type_fp& poly_to_search) {
  size_t current_ring = 0;
  size_t current_index = 0;
  for (const auto& poly : poly_to_search) {
    if (ring_indices[current_index] == current_ring) {
      if (is_intersecting(a, b, poly.outer())) {
        return true;
      }
      current_index++;
      if (current_index >= ring_indices.size()) {
        return false; // We've finished our search.
      }
    }
    for (const auto& inner : poly.inners()) {
      current_ring++;
      if (ring_indices[current_index] == current_ring) {
        if (is_intersecting(a, b, inner)) {
          return true;
        }
        current_index++;
        if (current_index >= ring_indices.size()) {
          return false; // We've finished our search.
        }
      }
    }
    current_ring++;
  }
  throw std::logic_error("There were more rings to search than rings.  "
                         "This should never happen.");
}

bool is_intersecting(
    const point_type_fp& a, const point_type_fp& b,
    const RingIndices& ring_indices,
    const nested_multipolygon_type_fp* poly_to_search) {
  size_t current_ring = 0;
  size_t current_index = 0;
  for (const auto& poly : *poly_to_search) {
    if (ring_indices[current_index].first == current_ring) {
      if (is_intersecting(a, b, ring_indices[current_index].second, poly.first)) {
         return true;
      }
      current_index++;
      if (current_index >= ring_indices.size()) {
         return false; // We're finished our search.
      }
    }
    for (const auto& inner : poly.second) {
      current_ring++;
      if (ring_indices[current_index].first == current_ring) {
        if (is_intersecting(a, b, ring_indices[current_index].second, inner)) {
           return true;
        }
        current_index++;
        if (current_index >= ring_indices.size()) {
           return false; // We're finished our search.
        }
      }
    }
    current_ring++;
  }
  throw std::logic_error("There were more rings to search than rings.  "
                         "This should never happen.");
}

// Return true if this edge from a to b is part of the path finding surface.
bool PathFindingSurface::in_surface(
    const point_type_fp& a, const point_type_fp& b,
    const RingIndices& ring_indices) const {
  if (b < a) {
    return in_surface(b, a, ring_indices);
  }
  const auto key = make_pair(a, b);
  auto memoized_result = in_surface_memo.find(key);
  if (memoized_result != in_surface_memo.cend()) {
    return memoized_result->second;
  }
  const nested_multipolygon_type_fp* poly_to_search;
  if (total_keep_in_grown) {
    poly_to_search = &*total_keep_in_grown;
  } else {
    poly_to_search = &keep_out_shrunk;
  }
  auto found_intersection = is_intersecting(a, b, ring_indices, poly_to_search);
  in_surface_memo.emplace(key, !found_intersection);
  return !found_intersection;
}

// Return all possible neighbors of current.  A neighbor can be
// start, end, or any of the points in all_vertices.  But only the
// ones that are in_surface are returned.
Neighbors PathFindingSurface::neighbors(const point_type_fp& start, const point_type_fp& end,
                                        const RingIndices& ring_indices,
                                        coordinate_type_fp g_score_current,
                                        const PathLimiter path_limiter,
                                        const point_type_fp& current) const {
  return Neighbors(start, end, current, ring_indices, g_score_current, path_limiter, this);
}

// Return a path from the start to the current.  Always return at
// least two points.
linestring_type_fp build_path(
    point_type_fp current,
    const unordered_map<point_type_fp, point_type_fp>& came_from) {
  linestring_type_fp result;
  while (came_from.count(current)) {
    result.push_back(current);
    current = came_from.at(current);
  }
  result.push_back(current);
  bg::reverse(result);
  return result;
}

optional<linestring_type_fp> PathFindingSurface::find_path(
    const point_type_fp& start, const point_type_fp& goal,
    const PathLimiter& path_limiter) const {
  RingIndices ring_indices;
  ring_indices = in_surface(start);
  if (ring_indices.size() == 0) {
    // Start is not in the surface.
    return boost::none;
  }
  if (ring_indices != in_surface(goal)) {
    // Either goal is not in the surface or it's in a region unreachable by start.
    return boost::none;
  }

  // Connect if a direct connection is possible.  This also takes care
  // of the case where start == goal.
  linestring_type_fp direct_ls;
  direct_ls.push_back(start);
  direct_ls.push_back(goal);
  try {
    if (in_surface(start, goal, ring_indices) &&
        (path_limiter == nullptr || !path_limiter(goal, bg::distance(start, goal)))) {
      return direct_ls;
    }
  } catch (GiveUp g) {
    return boost::none;
  }

  // Do astar.
  priority_queue<pair<coordinate_type_fp, point_type_fp>,
                 vector<pair<coordinate_type_fp, point_type_fp>>,
                 std::greater<pair<coordinate_type_fp, point_type_fp>>> open_set;
  open_set.emplace(bg::distance(start, goal), start);
  unordered_set<point_type_fp> closed_set;
  unordered_map<point_type_fp, point_type_fp> came_from;
  unordered_map<point_type_fp, coordinate_type_fp> g_score; // Empty should be considered infinity.
  g_score[start] = 0;
  while (!open_set.empty()) {
    const auto current = open_set.top().second;
    open_set.pop();
    if (current == goal) {
      // We're done.
      return make_optional(build_path(current, came_from));
    }
    if (closed_set.count(current) > 0) {
      // Skip this because we already "removed it", sort of.
      continue;
    }
    try {
      const auto current_neighbors = neighbors(
          start, goal, ring_indices, g_score.at(current), path_limiter, current);
      for (const auto& neighbor : current_neighbors) {
        const auto tentative_g_score = g_score.at(current) + bg::distance(current, neighbor);
        if (g_score.count(neighbor) == 0 || tentative_g_score < g_score.at(neighbor)) {
          // This path to neighbor is better than any previous one.
          came_from[neighbor] = current;
          g_score[neighbor] = tentative_g_score;
          open_set.emplace(tentative_g_score + bg::distance(neighbor, goal), neighbor);
        }
      }
    } catch (GiveUp g) {
      return boost::none;
    }
    // Because we can't delete from the open_set, we'll just marked
    // items as closed and ignore them later.
    closed_set.insert(current);
  }
  return boost::none;
}

} //namespace path_finding
