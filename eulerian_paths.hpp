#ifndef EULERIAN_PATHS_H
#define EULERIAN_PATHS_H

#include <vector>
#include <map>

#include "geometry.hpp"

/* This finds a minimal number of eulerian paths that cover the input.
 * The number of paths returned is equal to the number of vertices
 * with odd edge count divided by 2.
 *
 * To use, first get paths.  Each path is a vector of n points that
 * represents n-1 line segments.  Each path is considerd
 * bidirectional.
 *
 * After adding paths, build the Eulerian paths.  The resulting paths
 * cover all segments in the input paths with the minimum number of
 * paths as described above.
 */
template <typename point_p>
std::vector<std::vector<point_p>> get_eulerian_paths(std::vector<std::vector<point_p>> paths) {
   // Create a map from vertex to each path that starts or ends (or both) at that vertex.
  std::multimap<point_p, std::vector<point_p>> vertexToPath;
  for (auto path : paths) {
    if (path.size() == 0) {
      continue;
    }
    point_p start = path.front();
    point_p end = path.back();
    vertexToPath.insert(std::pair<point_p, std::vector<point_p>>(start, path));
    if (start != end) {
      vertexToPath.insert(std::pair<point_p, std::vector<point_p>>(end, path));
    }
  }
  printf("done!\n");
  for (auto& iter : vertexToPath) {
    for (auto& val : iter.second) {
      printf("%d, %d\n", iter.first, val);
    }
  }
  return std::vector<std::vector<point_p>>{};
}

#endif //EULERIAN_PATHS_H
