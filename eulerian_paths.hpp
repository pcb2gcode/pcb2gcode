#ifndef EULERIAN_PATHS_H
#define EULERIAN_PATHS_H

#include <vector>
using std::vector;

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
template <typename point>
vector<vector<point>> get_eulerian_paths(vector<vector<point>> paths);

#endif //EULERIAN_PATHS_H
