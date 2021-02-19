#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <boost/optional.hpp>
#include <unordered_map>

#include "geometry.hpp"
#include "bg_operators.hpp"
#include "segment_tree.hpp"

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
using SearchKey = size_t;
boost::optional<MPRingIndices> inside_multipolygon(const point_type_fp& p,
                                  const multi_polygon_type_fp& mp);
boost::optional<MPRingIndices> outside_multipolygon(const point_type_fp& p,
                                   const multi_polygon_type_fp& mp);
boost::optional<RingIndices> inside_multipolygons(
    const point_type_fp& p,
    const nested_multipolygon_type_fp& mp);
boost::optional<RingIndices> outside_multipolygons(
    const point_type_fp& p,
    const nested_multipolygon_type_fp& mp);

class Neighbors {
 public:
  class iterator {
   public:
    iterator(const Neighbors* neighbors, size_t point_index) :
      neighbors(neighbors),
      point_index(point_index) {}
    iterator operator++();
    bool operator!=(const iterator& other) const;
    bool operator==(const iterator& other) const;
    const point_type_fp& operator*() const;
   private:
    const Neighbors* neighbors;
    size_t point_index;
  };

  Neighbors(const point_type_fp& start, const point_type_fp& goal,
            const point_type_fp& current,
            const coordinate_type_fp& max_path_length,
            const std::vector<point_type_fp>& vertices,
            const PathFindingSurface* pfs);
  inline bool is_neighbor(const point_type_fp p) const;
  iterator begin() const;
  iterator end() const;
  const point_type_fp& start;
  const point_type_fp& goal;
  const point_type_fp& current;

 private:
  const coordinate_type_fp max_path_length_squared;
  const std::vector<point_type_fp>& vertices;
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
  const boost::optional<SearchKey>& in_surface(point_type_fp p) const;
  void decrement_tries() const;
  Neighbors neighbors(const point_type_fp& start, const point_type_fp& goal,
                      const coordinate_type_fp& max_path_length,
                      SearchKey search_key,
                      const point_type_fp& current) const;
  // Find a path from start to goal in the available surface, limited
  // in operations.
  boost::optional<linestring_type_fp> find_path(
      const point_type_fp& start, const point_type_fp& goal,
      const coordinate_type_fp& max_path_length,
      const boost::optional<size_t>& max_tries) const;
  // Find a path from start to goal in the available surface, limited
  // in operations.
  boost::optional<linestring_type_fp> find_path(
      const point_type_fp& start, const point_type_fp& goal,
      const coordinate_type_fp& max_path_length,
      const boost::optional<size_t>& max_tries,
      SearchKey search_key) const;
  const std::vector<point_type_fp>& vertices(SearchKey search_key) const;
  multi_polygon_type_fp get_surface() const;

 private:
  friend class Neighbors;
  bool in_surface(
      const point_type_fp& a, const point_type_fp& b) const;
  boost::optional<linestring_type_fp> find_path(
      const point_type_fp& start, const point_type_fp& goal,
      const coordinate_type_fp& max_path_length,
      SearchKey search_key) const;

  // Each shape corresponses to an element in all_vertices and they
  // are in the same order.  The boolean indicates if this is the
  // outer.  This is later used for computing the inside/outside of
  // each shape.
  boost::optional<nested_multipolygon_type_fp> total_keep_in_grown;
  nested_multipolygon_type_fp keep_out_shrunk;

  // all_vertices is one list for each ring in the original.  The
  // lists are arranged in the same way as the RingIndices.
  std::vector<std::vector<std::vector<point_type_fp>>> all_vertices;
  mutable std::unordered_map<std::pair<point_type_fp, point_type_fp>, bool> edge_in_surface_memo;
  // RingIndices can be very large and slow to hash so we'll store
  // them here and elsewhere just store the index into this list.
  mutable std::vector<RingIndices> ring_indices_cache;
  mutable std::unordered_map<RingIndices, size_t,
                             std::hash<RingIndices>,
                             std::equal_to<RingIndices>> ring_indices_lookup;
  mutable std::unordered_map<point_type_fp, boost::optional<SearchKey>> point_in_surface_memo;
  segment_tree::SegmentTree tree;
  mutable std::unordered_map<SearchKey, std::vector<point_type_fp>> vertices_memo;
  mutable boost::optional<size_t> tries; // This is not great to be mutable.
};

struct GiveUp {};

} //namespace path_finding

#endif //PATH_FINDING_H
