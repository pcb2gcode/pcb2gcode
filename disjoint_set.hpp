#ifndef DISJOINT_SET_HPP
#define DISJOINT_SET_HPP

#include <unordered_map>

template <typename node_t>
class DisjointSet {
 public:
  const node_t& find(const node_t& node) {
    const node_t *current = &node;
    if (parent.find(*current) == parent.cend()) {
      // New node
      parent[node] = node;
      rank[node] = 0;
      return node;
    }
    while (parent.at(*current) != *current) {
      const auto& p = parent[*current];
      const auto& pp = parent[p];
      parent[*current] = pp;
      current = &p;
    }
    return *current;
  }

  void join(const node_t& x, const node_t& y) {
    const auto& px = find(x);
    const auto& py = find(y);
    if (px == py) {
      return;
    }
    if (rank[x] >= rank[y]) {
      parent[y] = x;
      if (rank[x] == rank[y]) {
        rank[x]++;
      }
    }
  }
 private:
  // Stores the parent for each node.  If the parent isn't in the map
  // then the node is its own parent.
  std::unordered_map<node_t, node_t> parent;
  // Stores the rank of each node, which is an upper bound on the
  // height of the subtree below the node.  Nodes that aren't in the
  // nap are assumed to have a rank of 0.
  std::unordered_map<node_t, size_t> rank;
};

#endif // DISJOINT_SET_HPP
