#include "eulerian_paths.hpp"

#include <map>
using namespace std;

template <typename point>
vector<vector<point>> get_eulerian_paths(const vector<vector<point>>& paths) {
  // Create a map from vertex to each path that starts or ends (or both) at that vertex.
  multimap<point, vector<point>> vertexToPath;
  for (auto& path : paths) {
    point start = path.front();
    point end = path.back();
    vertexToPath.insert(start, path);
    if (!bg::equals(start, end)) {
      vertexToPath.insert(end, path);
    }
  }
  return nullptr;
}
