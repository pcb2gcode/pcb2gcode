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
#include "segment_tree.hpp"

namespace path_finding {

using boost::optional;
using boost::make_optional;

Neighbors::iterator Neighbors::iterator::operator++() {
  const auto& all_vertices_size = neighbors->vertices.size();
  do {
    // Move to a new valid point, even if it isn't a neighbor.
    point_index++;
  } while (point_index < all_vertices_size + 2 &&
           !neighbors->is_neighbor(**this));
  return *this;
}

bool Neighbors::iterator::operator!=(const Neighbors::iterator& other) const {
  return (point_index != other.point_index);
}

bool Neighbors::iterator::operator==(const Neighbors::iterator& other) const {
  return !(*this != other);
}

const point_type_fp& Neighbors::iterator::operator*() const {
  if (point_index == 0) {
    return neighbors->start;
  } else if (point_index == 1) {
    return neighbors->goal;
  } else {
    return neighbors->vertices[point_index-2];
  }
}

Neighbors::Neighbors(const point_type_fp& start, const point_type_fp& goal,
                     const point_type_fp& current,
                     const coordinate_type_fp& max_path_length,
                     const std::vector<point_type_fp>& vertices,
                     const PathFindingSurface* pfs) :
    start(start),
    goal(goal),
    current(current),
    max_path_length_squared(max_path_length),
    vertices(vertices),
    pfs(pfs) {}

// Returns a valid neighbor index that is either the one provided or
// the next higher valid one.
inline bool Neighbors::is_neighbor(const point_type_fp p) const {
  if (p == current) {
    return false;
  }
  pfs->decrement_tries();
  if (bg::distance(current, p) + bg::distance(p, goal) > max_path_length_squared) {
    return false;
  }
  if (!pfs->in_surface(current, p)) {
    return false;
  }
  return true;
}

Neighbors::iterator Neighbors::begin() const {
  auto ret = iterator(this, 0);
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
  return iterator(this, vertices.size()+2);
}

vector<pair<point_type_fp, point_type_fp>> get_all_segments(
    const vector<vector<std::reference_wrapper<const ring_type_fp>>>& all_rings) {
  vector<pair<point_type_fp, point_type_fp>> ret;
  for (const auto& x0 : all_rings) {
    for (auto x1 : x0) {
      for (size_t i = 0; i+1 < x1.get().size(); i++) {
        ret.emplace_back(x1.get()[i], x1.get()[i+1]);
      }
    }
  }
  return ret;
}

vector<std::reference_wrapper<const ring_type_fp>> get_all_rings(const multi_polygon_type_fp& mpolys) {
  vector<std::reference_wrapper<const ring_type_fp>> ret;
  for (const auto& poly : mpolys) {
    ret.push_back(poly.outer());
    for (const auto& inner : poly.inners()) {
      ret.push_back(inner);
    }
  }
  return ret;
}

vector<vector<std::reference_wrapper<const ring_type_fp>>> get_all_rings(
    const nested_multipolygon_type_fp& mpolys) {
  vector<vector<std::reference_wrapper<const ring_type_fp>>> ret;
  for (const auto& poly : mpolys) {
    ret.push_back(get_all_rings(poly.outer()));
    for (const auto& inner: poly.inners()) {
      ret.push_back(get_all_rings(inner));
    }
  }
  return ret;
}

PathFindingSurface::PathFindingSurface(const optional<multi_polygon_type_fp>& keep_in,
                                       const multi_polygon_type_fp& keep_out,
                                       const coordinate_type_fp tolerance) {
  if (keep_in) {
    multi_polygon_type_fp total_keep_in = *keep_in - keep_out;

    total_keep_in_grown.emplace();
    for (const auto& poly : total_keep_in) {
      all_vertices.emplace_back();
      all_vertices.back().emplace_back(poly.outer().cbegin(), poly.outer().cend());
      total_keep_in_grown->emplace_back(bg_helpers::buffer_miter(poly.outer(), tolerance));
      for (const auto& inner : poly.inners()) {
        all_vertices.back().emplace_back(inner.cbegin(), inner.cend());
        // Because the inner is reversed, we need to reverse it so
        // that the buffer algorithm won't get confused.
        auto temp_inner = inner;
        bg::reverse(temp_inner);
        // tolerance needs to be inverted because growing a shape
        // shrinks the holes in it.
        total_keep_in_grown->back().inners().push_back(bg_helpers::buffer_miter(temp_inner, -tolerance));
      }
    }
  } else {
    for (const auto& poly : keep_out){
      all_vertices.emplace_back();
      all_vertices.back().emplace_back(poly.outer().cbegin(), poly.outer().cend());
      keep_out_shrunk.emplace_back(bg_helpers::buffer_miter(poly.outer(), -tolerance));
      for (const auto& inner : poly.inners()) {
        all_vertices.back().emplace_back(inner.cbegin(), inner.cend());
        // Because the inner is reversed, we need to reverse it so
        // that the buffer algorithm won't get confused.
        auto temp_inner = inner;
        bg::reverse(temp_inner);
        // tolerance needs to be inverted because shrinking a shape
        // grows the holes in it.
        keep_out_shrunk.back().inners().push_back(bg_helpers::buffer_miter(temp_inner, tolerance));
      }
    }
  }
  const nested_multipolygon_type_fp& poly_to_search = total_keep_in_grown ? *total_keep_in_grown : keep_out_shrunk;
  const vector<vector<std::reference_wrapper<const ring_type_fp>>>& all_rings = get_all_rings(poly_to_search);
  const auto& all_segments = get_all_segments(all_rings);
  segment_tree::SegmentTree x(all_segments);
  tree = std::move(x);

  for (auto av : all_vertices) {
    sort(av.begin(), av.end());
    av.erase(std::unique(av.begin(), av.end()), av.end());
  }
}

boost::optional<MPRingIndices> inside_multipolygon(const point_type_fp& p,
                                                   const multi_polygon_type_fp& mp) {
  for (size_t poly_index = 0; poly_index < mp.size(); poly_index++) {
    const auto& poly = mp[poly_index];
    if (point_in_ring(p, poly.outer())) {
      // Might be part of this shape but only if the point isn't in
      // the inners.
      MPRingIndices ring_indices{{poly_index, {0}}};
      for (size_t inner_index = 0; inner_index < poly.inners().size(); inner_index++) {
        const auto& inner = poly.inners()[inner_index];
        if (!point_in_ring(p, inner)) {
          // We'll have to make sure not to cross this inner.
          ring_indices.back().second.emplace_back(inner_index+1);
        } else {
          break; // We're inside one of the inners so give up.
        }
      }
      if (ring_indices.back().second.size() == poly.inners().size() + 1) {
        // We never hit the break so we're inside this shape so we're
        // done.
        return {ring_indices};
      }
      // We're inside the outer but also inside an inner!  There might
      // be another shape inside this hold so we'll ignore this one
      // and keep searching.
    }
  }
  return boost::none;
}

boost::optional<MPRingIndices> outside_multipolygon(const point_type_fp& p,
                                                    const multi_polygon_type_fp& mp) {
  MPRingIndices ring_indices;
  for (size_t poly_index = 0; poly_index < mp.size(); poly_index++) {
    const auto& poly = mp[poly_index];
    if (point_in_ring(p, poly.outer())) {
      // We're inside the outer, maybe we're in an inner?  If not, we
      // aren't outside at all and we'll just give up.
      bool in_any_inner = false;
      for (size_t i = 0; i < poly.inners().size(); i++) {
        const auto& inner = poly.inners()[i];
        if (point_in_ring(p, inner)) {
          in_any_inner = true;
          ring_indices.emplace_back(poly_index, vector<size_t>{i+1});
          break;
        }
      }
      if (!in_any_inner) {
        // We're inside the outer but not in any of the inners, so
        // we're in the shape, but we want to be outside the shape, so
        // we've failed.
        return boost::none;
      }
    } else {
      // We need to keep out of this outer.
      ring_indices.emplace_back(poly_index, vector<size_t>{0});
      // No need to examine the inners which we can't possibly be inside.
    }
  }
  return {ring_indices};
}

boost::optional<RingIndices> inside_multipolygons(
    const point_type_fp& p,
    const nested_multipolygon_type_fp& mp) {
  for (size_t poly_index = 0; poly_index < mp.size(); poly_index++) {
    const auto& poly = mp[poly_index];
    boost::optional<MPRingIndices> inside_mp = inside_multipolygon(p, poly.outer());
    if (inside_mp) {
      // Might be part of this shape but only if the point isn't in
      // the inners.
      RingIndices ring_indices{{poly_index, {{0, *inside_mp}}}};
      for (size_t inner_index = 0; inner_index < poly.inners().size(); inner_index++) {
        const auto& inner = poly.inners()[inner_index];
        auto outside_mp = outside_multipolygon(p, inner);
        if (outside_mp) {
          // We'll have to make sure not to cross this inner.
          ring_indices.back().second.emplace_back(inner_index+1, *outside_mp);
        } else {
          break; // We're inside one of the inners so give up.
        }
      }
      if (ring_indices.back().second.size() == poly.inners().size() + 1) {
        // We never hit the break so we're inside this shape so we're
        // done.
        return {ring_indices};
      }
      // We're inside the outer but also inside an inner!  It might
      // be an outer in an inner so we'll ignore this one and keep
      // searching.
    }
  }
  return boost::none;
}

boost::optional<RingIndices> outside_multipolygons(
    const point_type_fp& p,
    const nested_multipolygon_type_fp& mp) {
  RingIndices ring_indices;
  for (size_t poly_index = 0; poly_index < mp.size(); poly_index++) {
    const auto& poly = mp[poly_index];
    auto outside_mp = outside_multipolygon(p, poly.outer());
    if (!outside_mp) {
      // We're inside the outer, maybe we're in an inner?  If not, we
      // aren't outside at all and we'll just give up.
      bool in_any_inner = false;
      for (size_t inner_index = 0; inner_index < poly.inners().size(); inner_index++) {
        const auto& inner = poly.inners()[inner_index];
        auto inside_mp = inside_multipolygon(p, inner);
        if (inside_mp) {
          in_any_inner = true;
          ring_indices.emplace_back(poly_index, vector<pair<size_t, MPRingIndices>>{{inner_index + 1, *inside_mp}});
          break;
        }
      }
      if (!in_any_inner) {
        // We're inside the outer but not in any of the inners, so
        // we're in the shape, but we want to be outside the shape, so
        // we've failed.
        return boost::none;
      }
    } else {
      // We need to keep out of this outer.
      ring_indices.emplace_back(poly_index, vector<pair<size_t, MPRingIndices>>{{0, *outside_mp}});
    }
  }
  return {ring_indices};
}

/* Given a point, determine if the point is in the search surface.  If
   so, return a non-default value, otherwise default value.  If two
   points return the same value, there is a path between them in the
   surface.  If not then there cannot be a path between them.  The
   value will actually be a vector of size_t that indicates which
   rings in the stored polygon should be used for the generated points
   in the path and also for the collision detection. */
const boost::optional<SearchKey>& PathFindingSurface::in_surface(point_type_fp p) const {
  auto memoized_result = point_in_surface_memo.find(p);
  if (memoized_result != point_in_surface_memo.cend()) {
    return memoized_result->second;
  }
  boost::optional<RingIndices> maybe_ring_indices;
  if (total_keep_in_grown) {
    maybe_ring_indices = inside_multipolygons(p, *total_keep_in_grown);
  } else {
    maybe_ring_indices = outside_multipolygons(p, keep_out_shrunk);
  }
  if (!maybe_ring_indices) {
    return point_in_surface_memo.emplace(p, boost::none).first->second;
  }
  const auto& ring_indices = *maybe_ring_indices;
  // Check if this one is already in the cache.
  const auto& find_result = ring_indices_lookup.find(std::cref(ring_indices));
  if (find_result != ring_indices_lookup.cend()) {
    // Found in the cache so we can use that.
    return point_in_surface_memo.emplace(p, find_result->second).first->second;
  }
  // Not found so we need to add it to the cache.
  ring_indices_cache.push_back(ring_indices);
  ring_indices_lookup.emplace(ring_indices_cache.back(), ring_indices_cache.size()-1);
  return point_in_surface_memo.emplace(p, ring_indices_cache.size()-1).first->second;
}

void PathFindingSurface::decrement_tries() const {
  if (tries) {
    if (*tries == 0) {
      throw GiveUp();
    }
    (*tries)--;
  }
}

// Return true if this edge from a to b is part of the path finding surface.
bool PathFindingSurface::in_surface(
    const point_type_fp& a, const point_type_fp& b) const {
  if (b < a) {
    return in_surface(b, a);
  }
  const auto key = make_pair(a, b);
  auto memoized_result = edge_in_surface_memo.find(key);
  if (memoized_result != edge_in_surface_memo.cend()) {
    return memoized_result->second;
  }
  auto found_intersection = tree.intersects(a, b);
  edge_in_surface_memo.emplace(key, !found_intersection);
  return !found_intersection;
}

// Return all possible neighbors of current.  A neighbor can be
// start, end, or any of the points in all_vertices.  But only the
// ones that are in_surface are returned.
Neighbors PathFindingSurface::neighbors(const point_type_fp& start, const point_type_fp& goal,
                                        const coordinate_type_fp& max_path_length,
                                        SearchKey search_key,
                                        const point_type_fp& current) const {
  return Neighbors(start, goal, current, max_path_length, vertices(search_key), this);
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
    const coordinate_type_fp& max_path_length,
    SearchKey search_key) const {
  // Connect if a direct connection is possible.  This also takes care
  // of the case where start == goal.
  try {
    if (in_surface(start, goal)) {
      decrement_tries();
      if (bg::comparable_distance(start, goal) < max_path_length * max_path_length) {
        // in_surface builds up some structures that are only efficient if
        // we're doing many tries.
        return {{start, goal}};
      }
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
          start, goal,
          max_path_length - g_score.at(current),
          search_key,
          current);
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

optional<linestring_type_fp> PathFindingSurface::find_path(
    const point_type_fp& start, const point_type_fp& goal,
    const coordinate_type_fp& max_path_length,
    const boost::optional<size_t>& max_tries,
    SearchKey search_key) const {
  if (max_tries) {
    if (*max_tries == 0) {
      return boost::none;
    }
    tries.emplace(*max_tries);
  } else {
    tries = boost::none;
  }
  return find_path(start, goal, max_path_length, search_key);
}

optional<linestring_type_fp> PathFindingSurface::find_path(
    const point_type_fp& start, const point_type_fp& goal,
    const coordinate_type_fp& max_path_length,
    const boost::optional<size_t>& max_tries) const {
  if (max_tries) {
    if (*max_tries == 0) {
      return boost::none;
    }
    tries.emplace(*max_tries);
  } else {
    tries = boost::none;
  }

  auto ring_indices = in_surface(start);
  if (!ring_indices) {
    // Start is not in the surface.
    return boost::none;
  }
  if (ring_indices != in_surface(goal)) {
    // Either goal is not in the surface or it's in a region unreachable by start.
    return boost::none;
  }
  return find_path(start, goal, max_path_length, *ring_indices);
}

const std::vector<point_type_fp>&
PathFindingSurface::vertices(SearchKey search_key) const {
  auto memoized_result = vertices_memo.find(search_key);
  if (memoized_result != vertices_memo.cend()) {
    return memoized_result->second;
  }
  std::vector<point_type_fp> ret;
  const auto& vertices = all_vertices;
  const auto& ring_indices = ring_indices_cache.at(search_key);
  for (size_t poly_index = 0; poly_index < ring_indices.size() ; poly_index++) {
    // This is the poly to look at.
    const auto& poly_ring_index = ring_indices[poly_index];
    // These are the vertices for that poly.
    const auto& poly_vertices = vertices[poly_ring_index.first];
    for (size_t ring_index = 0; ring_index < poly_ring_index.second.size(); ring_index++) {
      const auto& ring_ring_index = poly_ring_index.second[ring_index];
      const auto& ring_vertices = poly_vertices[ring_ring_index.first];
      ret.insert(ret.cend(), ring_vertices.cbegin(), ring_vertices.cend());
    }
  }
  return vertices_memo.emplace(search_key, ret).first->second;
}

} //namespace path_finding
