#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <boost/optional.hpp>

#include "geometry.hpp"

namespace path_finding {

// Find a path from start to end.  The path must stay within the keep_in area
// and stay outside the keep_out area, if those are provided.  The tolerance is
// a small value to shrink and grow those errors before checking for overlap.
boost::optional<linestring_type_fp> find_path(const point_type_fp& start, const point_type_fp& end,
                                              const boost::optional<multi_polygon_type_fp>& keep_in,
                                              const boost::optional<multi_polygon_type_fp>& keep_out,
                                              const coordinate_type_fp tolerance);
} //namespace path_finding

#endif //PATH_FINDING_H
