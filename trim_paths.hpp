#ifndef TRIM_PATHS_HPP
#define TRIM_PATHS_HPP

#include "geometry.hpp"

namespace trim_paths {

// Given toolpaths and backtracks, look for segments in toolspaths
// that match backtracks and remove them.  This makes the toolpaths
// smaller.  The backtracks are expected to be stright segments with
// just two vertices.
void trim_paths(std::vector<std::pair<linestring_type_fp, bool>>& toolpaths,
                const std::vector<std::pair<linestring_type_fp, bool>>& backtracks);

} // namespace trim_paths
  
#endif // TRIM_PATHS_HPP
