#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <boost/optional.hpp>

#include "geometry.hpp"
#include "bg_operators.hpp"

namespace path_finding {

// is_left(): tests if a point is Left|On|Right of an infinite line.
//    Input:  three points p0, p1, and p2
//    Return: >0 for p2 left of the line through p0 and p1
//            =0 for p2 on the line
//            <0 for p2 right of the line
//    See: Algorithm 1 "Area of Triangles and Polygons"
//    This is p0p1 cross p0p2.
extern inline coordinate_type_fp is_left(point_type_fp p0, point_type_fp p1, point_type_fp p2) {
  return ((p1.x() - p0.x()) * (p2.y() - p0.y()) -
          (p2.x() - p0.x()) * (p1.y() - p0.y()));
}

// Is x between a and b, where a can be lesser or greater than b.  If
// x == a or x == b, also returns true. */
extern inline coordinate_type_fp is_between(coordinate_type_fp a,
                                            coordinate_type_fp x,
                                            coordinate_type_fp b) {
  return x == a || x == b || (a-x>0) == (x-b>0);
}

// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
extern inline bool is_intersecting(const point_type_fp& p0, const point_type_fp& p1,
                                   const point_type_fp& p2, const point_type_fp& p3) {
  const coordinate_type_fp left012 = is_left(p0, p1, p2);
  const coordinate_type_fp left013 = is_left(p0, p1, p3);
  const coordinate_type_fp left230 = is_left(p2, p3, p0);
  const coordinate_type_fp left231 = is_left(p2, p3, p1);

  if (p0 != p1) {
    if (left012 == 0) {
      if (is_between(p0.x(), p2.x(), p1.x()) &&
          is_between(p0.y(), p2.y(), p1.y())) {
        return true; // p2 is on the line p0 to p1
      }
    }
    if (left013 == 0) {
      if (is_between(p0.x(), p3.x(), p1.x()) &&
          is_between(p0.y(), p3.y(), p1.y())) {
        return true; // p3 is on the line p0 to p1
      }
    }
  }
  if (p2 != p3) {
    if (left230 == 0) {
      if (is_between(p2.x(), p0.x(), p3.x()) &&
          is_between(p2.y(), p0.y(), p3.y())) {
        return true; // p0 is on the line p2 to p3
      }
    }
    if (left231 == 0) {
      if (is_between(p2.x(), p1.x(), p3.x()) &&
          is_between(p2.y(), p1.y(), p3.y())) {
        return true; // p1 is on the line p2 to p3
      }
    }
  }
  if ((left012 > 0) == (left013 > 0) ||
      (left230 > 0) == (left231 > 0)) {
    if (p1 == p2) {
      return true;
    }
    return false;
  } else {
    return true;
  }
}

class PathFindingSurface;

struct GiveUp {};

// Given a target location and a potential path length to get there, determine
// if it still makes sense to follow the path.  Return true if it does,
// otherwise false.
typedef std::function<bool(const point_type_fp& target, const coordinate_type_fp& length)> PathLimiter;

// Create a surface for doing path finding.  It can be used multiple times.  The
// surface available for paths is within the keep_in and also outside the
// keep_out.  If those are missing, they are ignored.  The tolerance should be a
// small epsilon value.
const std::shared_ptr<const PathFindingSurface> create_path_finding_surface(
    const boost::optional<multi_polygon_type_fp>& keep_in,
    const multi_polygon_type_fp& keep_out,
    const coordinate_type_fp tolerance);

boost::optional<int> in_polygon(const std::shared_ptr<const PathFindingSurface> path_finding_surface,
                                point_type_fp p);

// Find a path from start to end in the available surface.
boost::optional<linestring_type_fp> find_path(
    const std::shared_ptr<const PathFindingSurface> path_finding_surface,
    const point_type_fp& start, const point_type_fp& end,
    PathLimiter path_limiter);

} //namespace path_finding

#endif //PATH_FINDING_H
