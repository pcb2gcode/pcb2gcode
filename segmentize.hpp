#ifndef SEGMENTIZE_H
#define SEGMENTIZE_H

#include <vector>
#include <map>

#include "geometry.hpp"
#include <boost/polygon/isotropy.hpp>
#include <boost/polygon/segment_concept.hpp>
#include <boost/polygon/segment_utils.hpp>

namespace segmentize {
// Returns the sign of the input as -1,0,1 for negative/zero/positive.
template <typename T>
int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

/* Given a multi_linestring, return a new multiline_string where there
 * are no segments that cross any other segments.  Nor are there any T
 * shapes where the end of a linestring butts up against the center of
 * another.
 *
 * Repeats are removed but segments are considered directional.  For each
 * segment there is a boolean that is true if the segment is reversible.
 * Non-reversible segments are re-oriented if needed.  The default is reversible
 * true.
 */
std::vector<segment_type_p> segmentize(const std::vector<segment_type_p>& all_segments,
                                       const std::vector<bool>& allow_reversals) {
  std::vector<std::pair<size_t, segment_type_p>> intersected_segment_pairs;
  boost::polygon::intersect_segments(intersected_segment_pairs, all_segments.cbegin(), all_segments.cend());
  std::vector<segment_type_p> intersected_segments;
  for (const auto& p : intersected_segment_pairs) {
    const auto index_in_input = p.first;
    intersected_segments.push_back(p.second);
    const auto& allow_reversal = index_in_input < allow_reversals.size() ? allow_reversals[index_in_input] : true;
    if (!allow_reversal) {
      // Reverse segments that are now pointing in the wrong direction.
      const auto& input_segment = all_segments[index_in_input];
      auto& new_segment = intersected_segments.back();
      auto input_delta_x = input_segment.high().x() - input_segment.low().x();
      auto input_delta_y = input_segment.high().y() - input_segment.low().y();
      auto new_delta_x = new_segment.high().x() - new_segment.low().x();
      auto new_delta_y = new_segment.high().y() - new_segment.low().y();
      if (sgn(input_delta_x) != sgn(new_delta_x) ||
          sgn(input_delta_y) != sgn(new_delta_y)) {
        // Swap low and high.
        auto low = new_segment.low();
        new_segment.low(new_segment.high());
        new_segment.high(low);
      }
    }
  }
  std::sort(intersected_segments.begin(), intersected_segments.end());
  auto last = std::unique(intersected_segments.begin(), intersected_segments.end());
  intersected_segments.erase(last, intersected_segments.end());

  return intersected_segments;
}
} //namespace segmentize
#endif //SEGMENTIZE_H
