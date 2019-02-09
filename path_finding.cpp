#include <vector>
using std::vector;

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/astar_search.hpp>
#include <boost/optional.hpp>

#include "path_finding.hpp"

#include "geometry.hpp"
#include "bg_helpers.hpp"

using boost::optional;
using boost::make_optional;

using std::shared_ptr;
using std::make_shared;


class PathSurfaceBase {
 public:
  PathSurfaceBase(const optional<multi_polygon_type_fp>& keep_in,
                  const optional<multi_polygon_type_fp>& keep_out,
                  const coordinate_type_fp tolerance)
      :  keep_in_grown(keep_in ? make_optional(bg_helpers::buffer(*keep_in, tolerance)) : boost::none),
         keep_out_shrunk(keep_out ? make_optional(bg_helpers::buffer(*keep_out, -tolerance)) : boost::none),
         all_vertices(get_all_vertices(keep_in, keep_out)) {}
  const optional<multi_polygon_type_fp> keep_in_grown;
  const optional<multi_polygon_type_fp> keep_out_shrunk;
  const vector<point_type_fp> all_vertices;
 private:
  static vector<point_type_fp> get_all_vertices(const optional<multi_polygon_type_fp>& keep_in,
                                                const optional<multi_polygon_type_fp>& keep_out) {
    vector<point_type_fp> all_vertices;
    for (const auto& optional_mpoly : {keep_in, keep_out}) {
      if (optional_mpoly) {
        const auto& mpoly = *optional_mpoly;
        for (const auto& poly : mpoly) {
          for (const auto& point : poly.outer()) {
            all_vertices.push_back(point);
          }
          for (const auto& inner : poly.inners()) {
            for (const auto& point : inner) {
              all_vertices.push_back(point);
            }
          }
        }
      }
    }
    return all_vertices;
  }
};

// This is apoint surface that doesn't have a start and end in it.
class PathSurface {
 public:
  PathSurface(std::shared_ptr<PathSurfaceBase> base, const point_type_fp begin, const point_type_fp end)
      : base(base),
        begin(begin),
        end(end) {}
  // Return from all_vertices with begin==0 and end==1 and the rest shifted by 2.
  const point_type_fp& get_point_by_index(const size_t index) const {
    if (index == 0) {
      return begin;
    } if (index == 1) {
      return end;
    }
    return base->all_vertices[index-2];
  }
  const size_t points_num() const {
    return base->all_vertices.size() + 2;
  }
  bool in_surface(const point_type_fp& a, const point_type_fp& b) const {
    linestring_type_fp segment;
    segment.push_back(a);
    segment.push_back(b);
    if (base->keep_in_grown && !bg::covered_by(segment, *base->keep_in_grown)) {
      return false;
    }
    if (base->keep_out_shrunk && bg::intersects(segment, *base->keep_out_shrunk)) {
      return false;
    }
    return true;
  }
 private:
  std::shared_ptr<PathSurfaceBase> base;
  point_type_fp begin;
  point_type_fp end;
};

struct path_surface_traversal_catetory:
    public boost::vertex_list_graph_tag,
    public boost::incidence_graph_tag {};

class OutEdgeIteratorImpl;
class VertexIteratorImpl;

namespace boost {
/*
bool operator==(const point_type_fp& a, const point_type_fp& b) {
  return a.x() == b.x() && a.y() == b.y();
}

bool operator!=(const point_type_fp& a, const point_type_fp& b) {
  return a.x() != b.x() || a.y() != b.y();
}
*/
template <> struct graph_traits<PathSurface> {
  typedef size_t vertex_descriptor;
  typedef std::pair<vertex_descriptor, vertex_descriptor> edge_descriptor;
  typedef boost::undirected_tag directed_category;
  typedef boost::disallow_parallel_edge_tag edge_parallel_category;
  typedef path_surface_traversal_catetory traversal_category;

  typedef VertexIteratorImpl vertex_iterator;
  typedef OutEdgeIteratorImpl out_edge_iterator;
  typedef size_t degree_size_type;
  typedef size_t vertices_size_type;

  typedef void in_edge_iterator;
  typedef void edge_iterator;
  typedef void edges_size_type;
};
}

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
    while (target < path_surface->points_num() && // didn't fall of the target yet
           (target == source || // 0 length path
            !path_surface->in_surface(path_surface->get_point_by_index(source),  // segment is not in the allowed surface
                                      path_surface->get_point_by_index(target)))) {
      target++;
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

namespace boost {

template <> struct vertex_property_type<PathSurface> {
  typedef boost::graph_traits<PathSurface>::vertex_descriptor type;
};

} // namespace boost

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

namespace path_finding {

boost::optional<linestring_type_fp> find_path(const point_type_fp& start, const point_type_fp& end,
                                              const boost::optional<multi_polygon_type_fp>& keep_in,
                                              const boost::optional<multi_polygon_type_fp>& keep_out,
                                              const coordinate_type_fp tolerance) {
  boost::function_requires<boost::VertexListGraphConcept<PathSurface>>();
  boost::function_requires<boost::IncidenceGraphConcept<PathSurface>>();
  auto path_surface = make_shared<PathSurface>(make_shared<PathSurfaceBase>(PathSurfaceBase(keep_in,
                                                                                            keep_out,
                                                                                            tolerance)),
                                               start,
                                               end);

  // The predecessor map is a vertex-to-vertex mapping.
  typedef vector<Vertex> pred_map;
  pred_map predecessor(path_surface->points_num());
  //boost::iterator_property_map<pred_map::iterator, pred_map> pred_pmap(predecessor.begin());
  //auto pred_pmap = boost::make_iterator_property_map(predecessor.begin(), get(boost::vertex_index, *path_surface));
  // The distance map is a vertex-to-distance mapping.
  typedef vector<coordinate_type_fp> dist_map;
  dist_map distance(path_surface->points_num());;
  //boost::iterator_property_map<dist_map> dist_pmap(distance);

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
                 .distance_map(&distance[0])
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
  }
  return boost::none;
}

} //namespace path_finding
