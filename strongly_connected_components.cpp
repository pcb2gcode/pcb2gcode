#include <vector>
#include <stack>

#include "geometry.hpp"

#include "strongly_connected_components.hpp"

namespace std{

static inline bool operator<(const point_type_fp& x, const point_type_fp& y) {
  return std::tie(x.x(), x.y()) < std::tie(y.x(), y.y());
}

} // namespace std

static inline bool operator==(const point_type_fp& x, const point_type_fp& y) {
  return std::tie(x.x(), x.y()) == std::tie(y.x(), y.y());
}

static inline bool operator!=(const point_type_fp& x, const point_type_fp& y) {
  return std::tie(x.x(), x.y()) != std::tie(y.x(), y.y());
}

namespace strongly_connected_components {

using std::vector;
using std::pair;
using std::stack;
using std::map;
using std::min;

class strongly_connected_components_helper {
 private:
  size_t index = 0;
  stack<point_type_fp> s;
  map<point_type_fp, size_t> indices;
  map<point_type_fp, size_t> low_link;
  map<point_type_fp, bool> on_stack;
  map<point_type_fp, vector<point_type_fp>> graph;
  vector<vector<point_type_fp>> result;
  
  void strong_connect(point_type_fp v) {
    indices[v] = index;
    low_link[v] = index;
    index++;
    s.push(v);
    on_stack[v] = true;

    // Consider successors of v
    for (const auto& w : graph[v]) {
      if (indices.count(w) == 0) {
        // Successor w has not yet been visited; recurse on it
        strong_connect(w);
        low_link[v] = min(low_link[v], low_link[w]);
      } else if (on_stack[w]) {
        // Successor w is in stack S and hence in the current SCC
        // If w is not on stack, then (v, w) is an edge pointing to an SCC already found and must be ignored
        // Note: The next line may look odd - but is correct.
        // It says w.index not w.lowlink; that is deliberate and from the original paper
        low_link[v] = min(low_link[v], indices[w]);
      }
    }

    // If v is a root node, pop the stack and generate an SCC
    if (low_link[v] == indices[v]) {
      vector<point_type_fp> new_scc;
      do {
        const auto& w = s.top();
        s.pop();
        on_stack[w] = false;
        new_scc.push_back(w);
      } while(new_scc.back() != v);
      result.push_back(new_scc);
    }
  }

 public:
  strongly_connected_components_helper(const vector<pair<linestring_type_fp, bool>>& paths) {
    for (const auto& ls : paths) {
      graph[ls.first.front()].push_back(ls.first.back());
      if (ls.second) {
        // bi-directional
        graph[ls.first.back()].push_back(ls.first.front());
      }
    }
  }

  // Given a graph of edges where some edges might be directed and some
  // not and there may be edges which are loops, return the vertices of
  // the edges as a list of list of vertices.  Uses Tarjan's algorithm.
  vector<vector<point_type_fp>> get() {
    for (const auto& p : graph) {
      if (indices.count(p.first) == 0) {
        strong_connect(p.first);
      }
    }
    return result;
  }
};

// Given a graph of edges where some edges might be directed and some
// not and there may be edges which are loops, return the vertices of
// the edges as a list of list of vertices.
vector<vector<point_type_fp>> strongly_connected_components(
    const vector<pair<linestring_type_fp, bool>>& paths) {
  return strongly_connected_components_helper(paths).get();
}

} // namespace strongly_connected_components
