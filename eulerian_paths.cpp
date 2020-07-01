#include "geometry.hpp"
#include "bg_operators.hpp"
#include <vector>
#include <utility>

#include "segmentize.hpp"
#include "eulerian_paths.hpp"

namespace eulerian_paths {

using std::vector;
using std::pair;

// This calls segmentize and then get_eulerian_paths.  If unique is
// true, remove repeated segments.
multi_linestring_type_fp make_eulerian_paths(const multi_linestring_type_fp& paths, bool reversible, bool unique) {
  vector<pair<linestring_type_fp, bool>> path_to_simplify;
  for (const auto& ls : paths) {
    path_to_simplify.push_back(std::make_pair(ls, reversible));
  }
  path_to_simplify = segmentize::segmentize_paths(path_to_simplify);
  if (unique) {
    path_to_simplify = segmentize::unique(path_to_simplify);
  }
  auto eulerian_paths = get_eulerian_paths<
      point_type_fp,
      linestring_type_fp>(path_to_simplify);
  multi_linestring_type_fp ret;
  for (auto& eulerian_path : eulerian_paths) {
    ret.push_back(eulerian_path.first);
  }
  return ret;
}

} // namespace eulerian_paths
