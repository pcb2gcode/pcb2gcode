#include "path_finding.hpp"
#include <algorithm>                                     // for max, sort
#include <boost/graph/astar_search.hpp>                  // for astar_heuris...
#include <cstddef>                                       // for size_t, ptrd...
#include <tuple>                                         // for tie, operator<
#include <type_traits>                                   // for __decay_and_...
#include <unordered_map>                                 // for unordered_map
#include <utility>                                       // for make_pair, pair
#include <vector>                                        // for vector
#include "bg_helpers.hpp"                                // for buffer, oper...
#include "boost/concept_check.hpp"                       // for function_req...
#include "boost/geometry.hpp"                            // for polygon, cov...
#include "boost/graph/graph_concepts.hpp"                // for IncidenceGra...
#include "boost/graph/graph_traits.hpp"                  // for disallow_par...
#include "boost/graph/named_function_params.hpp"         // for bgl_named_pa...
#include "boost/none.hpp"                                // for none
#include "boost/operators.hpp"                           // for operator!=
#include "boost/property_map/function_property_map.hpp"  // for function_pro...
#include "boost/property_map/property_map.hpp"           // for typed_identi...
#include "geometry.hpp"                                  // for multi_polygo...
namespace path_finding { class OutEdgeIteratorImpl; }  // lines 23-23
namespace path_finding { class VertexIteratorImpl; }  // lines 24-24

using std::vector;
using std::unordered_map;

namespace path_finding {

using boost::optional;
using boost::make_optional;

using std::shared_ptr;
using std::make_shared;


class PathFindingSurface {
 public:
  PathFindingSurface(const multi_polygon_type_fp& keep_in,
                     const optional<multi_polygon_type_fp>& keep_out,
                     const coordinate_type_fp tolerance) {
    multi_polygon_type_fp total_keep_in;
    total_keep_in = keep_in;
    // By here, total_keep_in_grown is ready.
    if (keep_out) {
      total_keep_in = total_keep_in - *keep_out;
    }

    all_vertices.clear();
    for (const auto& poly : total_keep_in) {
      for (const auto& point : poly.outer()) {
        all_vertices.push_back(point);
      }
      for (const auto& inner : poly.inners()) {
        for (const auto& point : inner) {
          all_vertices.push_back(point);
        }
      }
    }

    sort(all_vertices.begin(),
         all_vertices.end(),
         [](const point_type_fp& a, const point_type_fp& b) {
           return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
         });
    all_vertices.erase(
        std::unique(all_vertices.begin(),
                    all_vertices.end(),
                      [](const point_type_fp& a, const point_type_fp& b) {
                        return std::tie(a.x(), a.y()) == std::tie(b.x(), b.y());
                      }),
        all_vertices.end());

    total_keep_in_grown = bg_helpers::buffer(total_keep_in, tolerance);
  }
  multi_polygon_type_fp total_keep_in_grown;
  vector<point_type_fp> all_vertices;
};

inline bool is_intersecting(const point_type_fp& p0, const point_type_fp& p1,
                            const point_type_fp& p2, const point_type_fp& p3) {
  const coordinate_type_fp s10_x = p1.x() - p0.x();
  const coordinate_type_fp s10_y = p1.y() - p0.y();
  const coordinate_type_fp s32_x = p3.x() - p2.x();
  const coordinate_type_fp s32_y = p3.y() - p2.y();

  coordinate_type_fp denom = s10_x * s32_y - s32_x * s10_y;
  if (denom == 0) {
    return false; // Collinear
  }
  const bool denomPositive = denom > 0;

  const coordinate_type_fp s02_x = p0.x() - p2.x();
  const coordinate_type_fp s02_y = p0.y() - p2.y();
  const coordinate_type_fp s_numer = s10_x * s02_y - s10_y * s02_x;
  if ((s_numer < 0) == denomPositive) {
    return false; // No collision
  }

  const coordinate_type_fp t_numer = s32_x * s02_y - s32_y * s02_x;
  if ((t_numer < 0) == denomPositive) {
    return false; // No collision
  }

  if (((s_numer > denom) == denomPositive) || ((t_numer > denom) == denomPositive)) {
    return false; // No collision
  }
  return true;
}

class PathSurface {
 public:
  PathSurface(const std::shared_ptr<const PathFindingSurface>& base, const point_type_fp begin, const point_type_fp end,
              PathLimiter path_limiter)
      : path_limiter(path_limiter) {
    total_keep_in_grown.clear();
    for (const auto& poly : base->total_keep_in_grown) {
      if (bg::covered_by(begin, poly) && bg::covered_by(end, poly)) {
        total_keep_in_grown.push_back(poly);
      }
    }
    all_vertices.clear();
    all_vertices.push_back(begin);
    all_vertices.push_back(end);
    for (const auto& point : base->all_vertices) {
      if (!bg::equals(begin, point) && !bg::equals(end, point)) {
        all_vertices.push_back(point);
      }
    }
    distance.resize(points_num());
  }

  const point_type_fp& get_point_by_index(const size_t index) const {
    return all_vertices[index];
  }

  const size_t points_num() const {
    return all_vertices.size();
  }

  // Returns true if there is at least some path from begin to end.
  bool has_path() {
    return total_keep_in_grown.size() > 0;
  }

  bool in_surface(const size_t& a_index, const size_t& b_index) const {
    if (a_index > b_index) {
      return in_surface(b_index, a_index);
    }
    auto a_b_hash = a_index * points_num() + b_index;
    auto memoized_result = in_surface_memo.find(a_b_hash);
    if (memoized_result != in_surface_memo.cend()) {
      return memoized_result->second;
    }
    const auto& point_a = get_point_by_index(a_index);
    const auto& point_b = get_point_by_index(b_index);
    for (const auto& poly : total_keep_in_grown) {
      const auto& ring = poly.outer();
      for (auto current = ring.cbegin(); current + 1 != ring.cend(); current++) {
        if (is_intersecting(point_a, point_b, *current, *(current+1))) {
          in_surface_memo.emplace(a_b_hash, false);
          return false;
        }
      }
      for (const auto& ring : poly.inners()) {
        for (auto current = ring.cbegin(); current + 1 != ring.cend(); current++) {
          if (is_intersecting(point_a, point_b, *current, *(current+1))) {
            in_surface_memo.emplace(a_b_hash, false);
            return false;
          }
        }
      }
    }
    in_surface_memo.emplace(a_b_hash, true);
    return true;
  }
  // The distance map is a vertex-to-distance mapping.
  vector<coordinate_type_fp> distance;
  const PathLimiter path_limiter;
 private:
  mutable std::unordered_map<size_t, bool> in_surface_memo;
  multi_polygon_type_fp total_keep_in_grown;
  vector<point_type_fp> all_vertices;
};

struct path_surface_traversal_catetory:
    public boost::vertex_list_graph_tag,
    public boost::incidence_graph_tag {};

} // namespace path_finding

namespace boost {

template <> struct graph_traits<path_finding::PathSurface> {
  typedef size_t vertex_descriptor;
  typedef std::pair<vertex_descriptor, vertex_descriptor> edge_descriptor;
  typedef boost::undirected_tag directed_category;
  typedef boost::disallow_parallel_edge_tag edge_parallel_category;
  typedef path_finding::path_surface_traversal_catetory traversal_category;

  typedef path_finding::VertexIteratorImpl vertex_iterator;
  typedef path_finding::OutEdgeIteratorImpl out_edge_iterator;
  typedef size_t degree_size_type;
  typedef size_t vertices_size_type;

  typedef void in_edge_iterator;
  typedef void edge_iterator;
  typedef void edges_size_type;
};
} // namespace boost

namespace path_finding {

typedef boost::graph_traits<PathSurface>::vertex_descriptor Vertex;
typedef boost::graph_traits<PathSurface>::edge_descriptor Edge;
typedef boost::graph_traits<PathSurface>::vertex_iterator VertexIterator;
typedef boost::graph_traits<PathSurface>::out_edge_iterator OutEdgeIterator;
typedef boost::graph_traits<PathSurface>::degree_size_type DegreeSizeType;
typedef boost::graph_traits<PathSurface>::vertices_size_type VerticesSizeType;

class OutEdgeIteratorImpl : public boost::forward_iterator_helper<OutEdgeIterator, Edge, std::ptrdiff_t, Edge*, Edge> {
 public:
  OutEdgeIteratorImpl()
      : source(0),
        target(0) {}
  OutEdgeIteratorImpl(const PathSurface& path_surface, const Vertex& source, const Vertex& target)
      : path_surface(&path_surface),
        source(source),
        target(target) {
    move_to_valid_target();
  }
  Edge operator*() const {
    return std::make_pair(source, target);
  }
  void operator++() {
      target++;
      move_to_valid_target();
  }
  bool operator==(const OutEdgeIterator& other) const {
    return path_surface == other.path_surface
        && source == other.source
        && target == other.target;
  }

 private:
  // Only advances if the current edge isn't in the surface or it's 0 length.
  void move_to_valid_target() {
    while (true) {
      if (target >= path_surface->points_num()) {
        break; // already fell off the end
      }
      if (target == source) {
        target++; // don't self-edge for sure
        continue;
      }
      if (path_surface->path_limiter &&
          path_surface->path_limiter(path_surface->get_point_by_index(target),
                                     path_surface->distance[source] +
                                     bg::distance(path_surface->get_point_by_index(source),
                                                  path_surface->get_point_by_index(target)))) {
        target++; // This path would be too long.
        continue;
      }
      if (!path_surface->in_surface(source, target)) {
        target++; // segment is not in the allowed surface
        continue;
      }
      break;
    }
  }
  const PathSurface* path_surface;
  Vertex source;
  Vertex target;
};

class VertexIteratorImpl : public boost::forward_iterator_helper<VertexIterator, Vertex, std::ptrdiff_t, Vertex*, Vertex> {
 public:
  VertexIteratorImpl()
      : current(0) {}
  VertexIteratorImpl(const Vertex current)
      : current(current) {}
  Vertex operator*() const {
    return current;
  }
  void operator++() {
      current++;
  }
  bool operator==(const VertexIterator& other) const {
    return current == other.current;
  }

 private:
  Vertex current;
};

std::pair<VertexIterator, VertexIterator> vertices(const PathSurface& path_surface) {
  return std::make_pair(0, path_surface.points_num());
}

std::pair<OutEdgeIterator, OutEdgeIterator> out_edges(const Vertex& vertex, const PathSurface& path_surface) {
  return std::make_pair(OutEdgeIteratorImpl(path_surface, vertex, 0), OutEdgeIteratorImpl(path_surface, vertex, path_surface.points_num()));
}

DegreeSizeType out_degree(Vertex v, const PathSurface& graph) {
  DegreeSizeType result = 0;

  std::pair<OutEdgeIterator, OutEdgeIterator> edges = out_edges(v, graph);
  for (OutEdgeIterator i = edges.first; i != edges.second; i++) {
    ++result;
  }

  return result;
}

Vertex source(Edge edge, const PathSurface&) {
  return edge.first;
}

Vertex target(Edge edge, const PathSurface&) {
  return edge.second;
}

VerticesSizeType num_vertices(const PathSurface& path_surface) {
  return path_surface.points_num();
}

} // namespace path_finding

namespace boost {

template <> struct vertex_property_type<path_finding::PathSurface> {
  typedef boost::graph_traits<path_finding::PathSurface>::vertex_descriptor type;
};

} // namespace boost

namespace path_finding {

class PathSurfaceHeuristic: public boost::astar_heuristic<PathSurface, coordinate_type_fp> {
 public:
  PathSurfaceHeuristic(shared_ptr<PathSurface> path_surface, const Vertex& goal)
      : path_surface(path_surface),
        goal(goal) {}

  int operator()(Vertex v) {
    return bg::distance(path_surface->get_point_by_index(v),
                        path_surface->get_point_by_index(goal));
  }

 private:
  const shared_ptr<PathSurface> path_surface;
  const Vertex goal;
};

struct FoundGoal {};

struct AstarGoalVisitor : public boost::default_astar_visitor {
  AstarGoalVisitor(Vertex goal)
      : goal(goal) {}

  void examine_vertex(Vertex u, const PathSurface&) {
    if (u == goal) {
      throw FoundGoal();
    }
  }

 private:
  Vertex goal;
};

const std::shared_ptr<const PathFindingSurface> create_path_finding_surface(
    const multi_polygon_type_fp& keep_in,
    const boost::optional<multi_polygon_type_fp>& keep_out,
    const coordinate_type_fp tolerance) {
  boost::function_requires<boost::VertexListGraphConcept<PathSurface>>();
  boost::function_requires<boost::IncidenceGraphConcept<PathSurface>>();
  return make_shared<PathFindingSurface>(keep_in, keep_out, tolerance);
}

boost::optional<linestring_type_fp> find_path(
    const std::shared_ptr<const PathFindingSurface> path_finding_surface,
    const point_type_fp& start, const point_type_fp& end,
    PathLimiter path_limiter) {
  linestring_type_fp direct_ls;
  direct_ls.push_back(start);
  direct_ls.push_back(end);
  if (bg::covered_by(direct_ls, path_finding_surface->total_keep_in_grown)) {
    return direct_ls;
  }
  auto path_surface = make_shared<PathSurface>(path_finding_surface,
                                               start, end,
                                               path_limiter);
  if (!path_surface->has_path()) {
    return boost::none;
  }
  // The predecessor map is a vertex-to-vertex mapping.
  vector<Vertex> predecessor(path_surface->points_num());

  // The weight map is the length of each segment.
  auto weight_pmap = boost::make_function_property_map<Edge>(
      [&path_surface](const Edge& edge) {
        return bg::distance(path_surface->get_point_by_index(edge.first),
                            path_surface->get_point_by_index(edge.second));
      });

  // The weight map is the length of each segment.
  auto vertex_index_pmap = boost::typed_identity_property_map<Vertex>();
  try {
    astar_search(*path_surface, 0, PathSurfaceHeuristic(path_surface, 1), // The end point is always made spot number 1 in the PathSurface.
                 boost::weight_map(weight_pmap)
                 .predecessor_map(&predecessor[0])
                 .distance_map(&path_surface->distance[0])
                 .vertex_index_map(vertex_index_pmap)
                 .visitor(AstarGoalVisitor(1)));
  } catch (FoundGoal f) {
    linestring_type_fp result;
    for (int i = 1; i != 0; i = predecessor[i]) {
      bg::append(result, path_surface->get_point_by_index(i));
    }
    bg::append(result, path_surface->get_point_by_index(0));
    bg::reverse(result);
    return make_optional(result);
  } catch (GiveUp g) {
    return boost::none;
  }
  return boost::none;
}

} //namespace path_finding
