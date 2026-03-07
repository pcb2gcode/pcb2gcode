#ifndef SEGMENT_TREE_HPP
#define SEGMENT_TREE_HPP

#include "geometry.hpp"
#include "value_ptr.hpp"
#include <vector>
#include <utility>
#include <boost/optional.hpp>

// A segment tree is initialized with a list of segments.  The
// segments can have any orientation and may have duplicates.  It can
// be queried: Find if any segments intersect with a given segment.
namespace segment_tree {

// segments are not directed so we'll always store them with first
// coordinate having the lower x and the second coordinate having the
// greater x.  If the x values are the same then they can be in either
// order.  The bool stores true if the first y is less than the second
// y.  Otherwise false.  If the y values are equal then it doesn't
// matter.
class segment_t {
 public:
  segment_t(point_type_fp a, point_type_fp b) {
    if (a.x() < b.x()) {
      first_ = a;
      second_ = b;
    } else {
      first_ = b;
      second_ = a;
    }
    positive_slope_ = first_.y() < second_.y();
  }
  inline const point_type_fp& first() const { return first_; }
  inline const point_type_fp& second() const { return second_; }
  inline coordinate_type_fp min_x() const { return first_.x(); }
  inline coordinate_type_fp max_x() const { return second_.x(); }
  inline coordinate_type_fp min_y() const { return positive_slope_ ? first_.y() : second_.y(); }
  inline coordinate_type_fp max_y() const { return positive_slope_ ? second_.y() : first_.y(); }
 private:
  point_type_fp first_;
  point_type_fp second_;
  bool positive_slope_;
};

struct Node {
  Node(coordinate_type_fp intercept,
       smart_ptr::value_ptr<Node> in,
       smart_ptr::value_ptr<Node> out) :
    intercept(intercept),
    in(std::move(in)),
    out(std::move(out)) {}
  Node(segment_t segment) :
    segment(segment) {}
  const coordinate_type_fp intercept = 0; // Where it crosses the axis.
  const smart_ptr::value_ptr<Node> in; // Edges that match the criteria above.
  const smart_ptr::value_ptr<Node> out; // Edges that don't match the criteria above.
  const boost::optional<segment_t> segment; // The segment at this node (leaves only).
};

class SegmentTree {
 public:
  SegmentTree(const SegmentTree&) = delete;
  SegmentTree& operator=(const SegmentTree&) = delete;
  SegmentTree(SegmentTree&&) = default;
  SegmentTree& operator=(SegmentTree&&) = default;

  SegmentTree(const std::vector<std::pair<point_type_fp, point_type_fp>>& segments = {});
  void print();
  bool intersects(const point_type_fp& p0, const point_type_fp& p1) const;
 private:
  smart_ptr::value_ptr<Node> root;
};

} //namespace segment_tree

#endif //SEGMENT_TREE_HPP
