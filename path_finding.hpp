#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <boost/optional.hpp>
#include "bg_operators.hpp"
#include <unordered_map>

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

// From: http://geomalgorithms.com/a03-_inclusion.html
extern inline bool point_in_ring(const point_type_fp& point, const ring_type_fp& ring) {
  int winding_number = 0;

  // loop through all edges of the polygon
  for (size_t i=0; i < ring.size()-1; i++) {
    if (ring[i].y() <= point.y()) {                   // start y <= point.y
      if (ring[i+1].y() > point.y()) {                // an upward crossing
        if (is_left(ring[i], ring[i+1], point) > 0) { // point left of  edge
          ++winding_number;                           // have a valid up intersect
        }
      }
    } else {                                          // start y > point.y
      if (ring[i+1].y() <= point.y()) {               // a downward crossing
        if (is_left(ring[i], ring[i+1], point) < 0) { // P right of  edge
          --winding_number;                           // have a valid down intersect
        }
      }
    }
  }
  return winding_number != 0;
}

class nested_polygon_type_fp {
 public:
  nested_polygon_type_fp(
      const multi_polygon_type_fp& outer_,
      const std::vector<multi_polygon_type_fp> inners_) :
    outer_(outer_),
    inners_(inners_) {}
  nested_polygon_type_fp(
      const multi_polygon_type_fp& outer_) :
    outer_(outer_) {}
  const multi_polygon_type_fp& outer() const { return outer_; }
  multi_polygon_type_fp& outer() { return outer_; }
  const std::vector<multi_polygon_type_fp>& inners() const { return inners_; }
  std::vector<multi_polygon_type_fp>& inners() { return inners_; }
 private:
  multi_polygon_type_fp outer_;
  std::vector<multi_polygon_type_fp> inners_;
};

using nested_multipolygon_type_fp = std::vector<nested_polygon_type_fp>;


using MPRingIndices = std::vector<std::pair<size_t, std::vector<size_t>>>;
using RingIndices = std::vector<std::pair<size_t, std::vector<std::pair<size_t, MPRingIndices>>>>;

MPRingIndices inside_multipolygon(const point_type_fp& p,
                                  const multi_polygon_type_fp& mp);
MPRingIndices outside_multipolygon(const point_type_fp& p,
                                   const multi_polygon_type_fp& mp);
RingIndices inside_multipolygons(
    const point_type_fp& p,
    const nested_multipolygon_type_fp& mp);
RingIndices outside_multipolygons(
    const point_type_fp& p,
    const nested_multipolygon_type_fp& mp);

// Given a target location and a potential path length to get there, determine
// if it still makes sense to follow the path.  Return true if it does,
// otherwise false.
using PathLimiter = std::function<bool(const point_type_fp& target, const coordinate_type_fp& length)>;

class Neighbors {
 public:
  class iterator {
   public:
    iterator(Neighbors* neighbors, size_t ring_index, size_t point_index) :
      neighbors(neighbors),
      ring_index(ring_index),
      point_index(point_index) {}
    iterator operator++();
    bool operator!=(const iterator& other) const;
    bool operator==(const iterator& other) const;
    const point_type_fp& operator*();
   private:
    Neighbors* neighbors;
    // 0 means start, 1 means goal, everything else is an index into a ring +2.
    size_t ring_index;
    // Ignored for start and goal, otherwise an index into a ring.
    size_t point_index;
  };

  Neighbors(const point_type_fp& start, const point_type_fp& goal,
            const point_type_fp& current,
            const RingIndices& ring_indices,
            coordinate_type_fp g_score_current,
            size_t path_finding_limit,
            coordinate_type_fp max_g1_distance,
            size_t& tries,
            const PathFindingSurface* pfs);
  bool path_limiter(const point_type_fp& waypoint, const coordinate_type_fp& length_so_far);
  bool is_neighbor(const point_type_fp p);
  iterator begin();
  iterator end();

 private:
  point_type_fp start;
  point_type_fp goal;
  point_type_fp current;
  RingIndices ring_indices;
  coordinate_type_fp g_score_current;
  size_t path_finding_limit;
  coordinate_type_fp max_g1_distance;
  size_t& tries;
  const PathFindingSurface* pfs;
};

class PathFindingSurface {
 public:
  // Create a surface for doing path finding.  It can be used multiple times.  The
  // surface available for paths is within the keep_in and also outside the
  // keep_out.  If those are missing, they are ignored.  The tolerance should be a
  // small epsilon value.
  PathFindingSurface(const boost::optional<multi_polygon_type_fp>& keep_in,
                     const multi_polygon_type_fp& keep_out,
                     const coordinate_type_fp tolerance);
  RingIndices in_surface(point_type_fp p) const;
  bool in_surface(
      const point_type_fp& a, const point_type_fp& b,
      const RingIndices& ring_indices) const;
  Neighbors neighbors(const point_type_fp& start, const point_type_fp& goal,
                      const RingIndices& ring_indices,
                      coordinate_type_fp g_score_current,
                      size_t path_finding_limit,
                      coordinate_type_fp max_g1_distance,
                      size_t& tries,
                      const point_type_fp& current) const;
  // Find a path from start to goal in the available surface.
  boost::optional<linestring_type_fp> find_path(
      const point_type_fp& start, const point_type_fp& goal,
      size_t path_finding_limit,
      coordinate_type_fp max_g1_distance) const;
  const std::vector<std::vector<point_type_fp>>& vertices() const { return all_vertices; };

 private:
  // Each shape corresponses to an element in all_vertices and they
  // are in the same order.  The boolean indicates if this is the
  // outer.  This is later used for computing the inside/outside of
  // each shape.
  boost::optional<nested_multipolygon_type_fp> total_keep_in_grown;
  nested_multipolygon_type_fp keep_out_shrunk;

  // all_vertices is one list for each ring in the original.  The list
  // are in DFS order of the original poly, with outer before inners.
  std::vector<std::vector<point_type_fp>> all_vertices;
  mutable std::unordered_map<std::pair<point_type_fp, point_type_fp>, bool> in_surface_memo;
};

struct GiveUp {};

} //namespace path_finding

#endif //PATH_FINDING_H
