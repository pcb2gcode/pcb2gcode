#ifndef SEGMENTIZE_H
#define SEGMENTIZE_H

#include <vector>
#include <map>

#include "geometry.hpp"
#include <boost/polygon/isotropy.hpp>
#include <boost/polygon/segment_concept.hpp>
#include <boost/polygon/segment_utils.hpp>

namespace segmentize {

// Return a vector of unique linestrings.  Each input linestring must have just 2 points.
std::vector<std::pair<linestring_type_fp, bool>> unique(
    const std::vector<std::pair<linestring_type_fp, bool>>& lss);

/* Convert each linestring, which might have multiple points in it,
 * into a linestrings that have just two points, the start and the
 * end.  Directionality is maintained on each one along with whether
 * or not it is reversible.
 */
std::vector<std::pair<linestring_type_fp, bool>> segmentize_paths(
    const std::vector<std::pair<linestring_type_fp, bool>>& toolpaths);

} //namespace segmentize
#endif //SEGMENTIZE_H
