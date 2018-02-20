#ifndef SEGMENTIZE_H
#define SEGMENTIZE_H

#include <vector>
#include <map>

#include "geometry.hpp"
#include <boost/polygon/isotropy.hpp>
#include <boost/polygon/segment_concept.hpp>
#include <boost/polygon/segment_utils.hpp>

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

#endif //SEGMENTIZE_H
