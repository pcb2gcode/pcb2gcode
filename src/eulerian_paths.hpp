#ifndef EULERIAN_PATHS_H
#define EULERIAN_PATHS_H

#include <vector>

#include "geometry.hpp"

namespace eulerian_paths {

template <typename point_t, typename linestring_t>
bool check_eulerian_paths(const std::vector<std::pair<linestring_t, bool>>& before,
                          const std::vector<std::pair<linestring_t, bool>>& after);

// Returns a minimal number of toolpaths that include all the milling in the
// oroginal toolpaths.  Each path is traversed once.  Each path has a bool
// indicating if the path is reversible.
template <typename point_t, typename linestring_t>
std::vector<std::pair<linestring_t, bool>> get_eulerian_paths(const std::vector<std::pair<linestring_t, bool>>& paths);

multi_linestring_type_fp make_eulerian_paths(const multi_linestring_type_fp& paths, bool reversible, bool unique);

} // namespace eulerian_paths
#endif //EULERIAN_PATHS_H
