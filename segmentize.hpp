#ifndef SEGMENTIZE_H
#define SEGMENTIZE_H

#include <vector>
#include <map>

#include "geometry.hpp"
#include <boost/polygon/isotropy.hpp>
#include <boost/polygon/segment_concept.hpp>
#include <boost/polygon/segment_utils.hpp>

namespace segmentize {
/* Given a multi_linestring, return a new multiline_string where there
 * are no segments that cross any other segments.  Nor are there any T
 * shapes where the end of a linestring butts up against the center of
 * another.
 *
 * Repeats are removed, as are segments that are identical but
 * pointing in opposite directions.
 */
std::vector<segment_type_p> segmentize(const std::vector<segment_type_p>& all_segments) {
  std::vector<segment_type_p> intersected_segments;
  boost::polygon::intersect_segments(intersected_segments, all_segments.cbegin(), all_segments.cend());
  // Sort the segments themselves.
  std::sort(intersected_segments.begin(), intersected_segments.end());
  auto last = std::unique(intersected_segments.begin(), intersected_segments.end());
  intersected_segments.erase(last, intersected_segments.end());

  return intersected_segments;
}

template <typename T>
int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

/* Given a multi_linestring, return a new multiline_string where there
 * are no segments that cross any other segments.  Nor are there any T
 * shapes where the end of a linestring butts up against the center of
 * another.
 *
 * Repeats are removed but segments are considered directional.
 */
std::vector<segment_type_p> segmentize_directed(const std::vector<segment_type_p>& all_segments) {
  std::vector<std::pair<size_t, segment_type_p>> intersected_segment_pairs;
  boost::polygon::intersect_segments(intersected_segment_pairs, all_segments.cbegin(), all_segments.cend());
  std::vector<segment_type_p> intersected_segments;
  for (const auto& p : intersected_segment_pairs) {
    intersected_segments.push_back(p.second);
    // Reverse segments that are now pointing in the wrong direction.
    const auto& input_segment = all_segments[p.first];
    auto& new_segment = intersected_segments.back();
    if (sgn(input_segment.high().x() - input_segment.low().x()) !=
        sgn(new_segment.high().x() - new_segment.low().x()) ||
        sgn(input_segment.high().y() - input_segment.low().y()) !=
        sgn(new_segment.high().y() - new_segment.low().y())) {
      auto low = new_segment.low();
      new_segment.low(new_segment.high());
      new_segment.high(low);
    }
  }
  std::sort(intersected_segments.begin(), intersected_segments.end());
  auto last = std::unique(intersected_segments.begin(), intersected_segments.end());
  intersected_segments.erase(last, intersected_segments.end());

  return intersected_segments;
}
} //namespace segmentize
#endif //SEGMENTIZE_H
