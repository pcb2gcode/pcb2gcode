#ifndef SEGMENTIZE_H
#define SEGMENTIZE_H

#include <vector>
#include <map>

#include "geometry.hpp"
#include <boost/polygon/isotropy.hpp>
#include <boost/polygon/segment_concept.hpp>
#include <boost/polygon/segment_utils.hpp>

typedef boost::polygon::point_data<coordinate_type> point_type_p;
typedef boost::polygon::point_data<coordinate_type_fp> point_type_fp_p;
typedef boost::polygon::segment_data<coordinate_type> segment_type_p;
typedef boost::polygon::segment_data<coordinate_type_fp> segment_type_fp_p;

/* Given a multi_linestring, return a new multiline_string where there
 * are no segments that cross any other segments.  Nor are there any T
 * shapes where the end of a linestring butts up against the center of
 * another.
 *
 * We do our best but because of floating point, we might get some
 * near crosses that are missed.
 */
std::vector<segment_type_fp_p> segmentize(const std::vector<segment_type_fp_p>& all_segments) {
  std::vector<segment_type_fp_p> intersected_segments;
  boost::polygon::intersect_segments(intersected_segments, all_segments.cbegin(), all_segments.cend());
  /* // Let's remoe duplicates from the segments.
  for (auto& segment : intersected_segments) {
    if (bg::get<0>(segment) < bg::get<1>(segment)) {
      const auto temp = bg::get<0>(segment);
      bg::set<0>(segment, bg::get<1>(segment));
      bg::set<1>(segment, temp);
    }
  }
  std::sort(intersected_segments.begin(), intersected_segments.end());
  auto last = std::unique(intersected_segments.begin(), intersected_segments.end());
  intersected_segments.erase(last, intersected_segments.end());*/
  return intersected_segments;
}

#endif //SEGMENTIZE_H
