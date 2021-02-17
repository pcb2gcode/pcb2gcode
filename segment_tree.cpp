#include <vector>
using std::vector;

#include <utility>
using std::pair;

#include <boost/optional.hpp>

#include "bg_operators.hpp"

#include "segment_tree.hpp"
#include "value_ptr.hpp"
using smart_ptr::value_ptr;
using smart_ptr::make_value;

namespace segment_tree {

template <bool on_x, bool less_than>
value_ptr<Node> make_node(vector<segment_t>::iterator segments_begin,
                          vector<segment_t>::iterator segments_end) {
  if (segments_end - segments_begin == 1) {
    // We're done.
    return make_value<Node>(*segments_begin);
  }
  // Sort the segments as specified by on_x and less_than.
  coordinate_type_fp (segment_t::*corner_axis_selector)() const;
  coordinate_type_fp factor;
  if (less_than && on_x) {
    corner_axis_selector = &segment_t::max_x;
    factor = 1;
  } else if (less_than && !on_x) {
    corner_axis_selector = &segment_t::max_y;
    factor = 1;
  } else if (!less_than && on_x) {
    corner_axis_selector = &segment_t::min_x;
    factor = -1;
  } else if (!less_than && !on_x) {
    corner_axis_selector = &segment_t::min_y;
    factor = -1;
  }
  std::sort(segments_begin, segments_end,
            [&](const segment_t& s0,
                const segment_t& s1) {
              return factor * (s0.*corner_axis_selector)() < factor * (s1.*corner_axis_selector)();
            });
  // Find the middle.  It will round down.  You can't add begin and
  // end, it might overflow.  Behavior is undefined.
  auto mid = segments_begin + (segments_end - segments_begin)/2;

  const auto new_intercept = ((*mid).*corner_axis_selector)();
  constexpr auto new_on_x = less_than ^ on_x;
  constexpr auto new_less_than = !less_than;
  // mid will get changed below so we neeed to store the intercept.
  return make_value<Node>(new_intercept,
                          make_node<new_on_x, new_less_than>(segments_begin, mid),
                          make_node<new_on_x, new_less_than>(mid, segments_end));
}

constexpr bool START_ON_X = true;
constexpr bool START_LESS_THAN = true;

SegmentTree::SegmentTree(const vector<std::pair<point_type_fp, point_type_fp>>& segments_in) {
  // For each segment, find the bounding box.
  vector<segment_t> segments;
  segments.reserve(segments_in.size());
  for (const auto& segment : segments_in) {
    segments.emplace_back(segment.first, segment.second);
  }
  if (segments.size() > 0) {
    root = make_node<START_ON_X, START_LESS_THAN>(segments.begin(), segments.end());
  }
}

// is_left(): tests if a point is Left|On|Right of an infinite line.
//    Input:  three points p0, p1, and p2
//    Return: >0 for p2 left of the line through p0 and p1
//            =0 for p2 on the line
//            <0 for p2 right of the line
//    See: Algorithm 1 "Area of Triangles and Polygons"
//    This is p0p1 cross p0p2.
static inline coordinate_type_fp is_left(point_type_fp p0, point_type_fp p1, point_type_fp p2) {
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
static inline bool is_intersecting(const point_type_fp& p0, const point_type_fp& p1,
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

template <bool on_x, bool less_than>
bool intersects(const segment_t& segment, const value_ptr<Node>& node) {
  if (node->segment) {
    return is_intersecting(segment.first(), segment.second(),
                           node->segment->first(), node->segment->second());
  }
  constexpr auto new_on_x = less_than ^ on_x;
  constexpr auto new_less_than = !less_than;
  if (intersects<new_on_x, new_less_than>(segment, node->out)) {
    return true;
  }
  coordinate_type_fp (segment_t::*corner_axis_selector)() const;
  coordinate_type_fp factor;
  if (less_than && on_x) {
    corner_axis_selector = &segment_t::min_x;
    factor = -1;
  } else if (less_than && !on_x) {
    corner_axis_selector = &segment_t::min_y;
    factor = -1;
  } else if (!less_than && on_x) {
    corner_axis_selector = &segment_t::max_x;
    factor = 1;
  } else if (!less_than && !on_x) {
    corner_axis_selector = &segment_t::max_y;
    factor = 1;
  }
  if (!(factor * (segment.*corner_axis_selector)() < factor * node->intercept)) {
    return intersects<new_on_x, new_less_than>(segment, node->in);
  }
  return false;
}

bool SegmentTree::intersects(const point_type_fp& p0, const point_type_fp& p1) const {
  if (root == nullptr) {
    return false;
  }
  return segment_tree::intersects<START_ON_X, START_LESS_THAN>(segment_t(p0, p1), root);
}

template <bool on_x, bool less_than>
void print_node(const value_ptr<Node>& node, std::string indent = "") {
  if (node->segment) {
    std::cout << indent << bg::wkt(node->segment->first()) << " "
              << bg::wkt(node->segment->second()) << std::endl;
    return;
  }
  std::cout << indent << "if all "
            << (on_x ? "x" : "y") << " is " << (!less_than ? "less than" : "greater than")
            << " " << node->intercept << " then:" << std::endl;
  constexpr auto new_on_x = less_than ^ on_x;
  constexpr auto new_less_than = !less_than;
  print_node<new_on_x, new_less_than>(node->out, indent + "  ");
  std::cout << indent << "else the above and:" << std::endl;
  print_node<new_on_x, new_less_than>(node->in, indent + "  ");
}

void SegmentTree::print() {
  print_node<START_ON_X, START_LESS_THAN>(root);
}

} //namespace path_finding
