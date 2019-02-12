#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <boost/optional.hpp>

#include "geometry.hpp"

namespace path_finding {

class PathFindingSurface;

// Create a surface for doing path finding.  It can be used multiple times.  The
// surface available for paths is within the keep_in and also outside the
// keep_out.  If those are missing, they are ignored.  The tolerance should be a
// small epsilon value.
const std::shared_ptr<const PathFindingSurface> create_path_finding_surface(
    const multi_polygon_type_fp& keep_in,
    const boost::optional<multi_polygon_type_fp>& keep_out,
    const coordinate_type_fp tolerance);


// Find a path from start to end in the available surface.
boost::optional<linestring_type_fp> find_path(
    const std::shared_ptr<const PathFindingSurface> path_finding_surface,
    const point_type_fp& start, const point_type_fp& end);

} //namespace path_finding

#endif //PATH_FINDING_H
