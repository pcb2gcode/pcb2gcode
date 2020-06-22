#include <vector>
#include <map>

#include "geometry_int.hpp"
#include "geometry.hpp"
#include "merge_near_points.hpp"
#include <boost/polygon/isotropy.hpp>
#include <boost/polygon/segment_concept.hpp>
#include <boost/polygon/segment_utils.hpp>

namespace segmentize {

using std::vector;
using std::pair;
using std::make_pair;
using std::sort;
using std::unique;

// For use when we have to convert from float to long and back.
const double SCALE = 1000000.0;

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
 * For each segment there is a boolean that is true if the segment is
 * reversible.  Non-reversible segments are re-oriented if needed.
 */
static inline vector<pair<segment_type_p, bool>> segmentize(
    const vector<segment_type_p>& all_segments,
    const vector<bool>& allow_reversals) {
  vector<pair<size_t, segment_type_p>> intersected_segment_pairs;
  boost::polygon::intersect_segments(intersected_segment_pairs, all_segments.cbegin(), all_segments.cend());
  vector<pair<segment_type_p, bool>> intersected_segments;
  for (const auto& p : intersected_segment_pairs) {
    const auto index_in_input = p.first;
    const auto& allow_reversal = allow_reversals[index_in_input];
    intersected_segments.push_back(make_pair(p.second, allow_reversal));
    if (!allow_reversal) {
      // Reverse segments that are now pointing in the wrong direction.
      const auto& input_segment = all_segments[index_in_input];
      auto& new_segment = intersected_segments.back().first;
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

  return intersected_segments;
}

// Convert each linestring, which might have multiple points in it,
// into a linestrings that have just two points, the start and the
// end.  Directionality is maintained on each one along with whether
// or not it is reversible.
vector<pair<linestring_type_fp, bool>> segmentize_paths(const vector<pair<linestring_type_fp, bool>>& toolpaths) {
  // Merge points that are very close to each other because it makes
  // us more likely to find intersections that was can use.
  auto merged_toolpaths = toolpaths;
  merge_near_points(merged_toolpaths, 0.00001);

  // First we need to split all paths so that they don't cross.  We need to
  // scale them up because the input is not floating point.
  vector<segment_type_p> all_segments;
  vector<bool> allow_reversals;
  for (const auto& toolpath_and_allow_reversal : merged_toolpaths) {
    const auto& toolpath = toolpath_and_allow_reversal.first;
    for (size_t i = 1; i < toolpath.size(); i++) {
      all_segments.push_back(
          segment_type_p(
              point_type_p(toolpath[i-1].x() * SCALE, toolpath[i-1].y() * SCALE),
              point_type_p(toolpath[i  ].x() * SCALE, toolpath[i  ].y() * SCALE)));
      allow_reversals.push_back(toolpath_and_allow_reversal.second);
    }
  }
  vector<pair<segment_type_p, bool>> split_segments = segmentize(all_segments, allow_reversals);

  // Only allow reversing the direction of travel if mill_feed_direction is
  // ANY.  We need to scale them back down.
  vector<pair<linestring_type_fp, bool>> segments_as_linestrings;
  segments_as_linestrings.reserve(split_segments.size());
  for (const auto& segment_and_allow_reversal : split_segments) {
    // Make a little 1-edge linestrings.
    linestring_type_fp ls;
    const auto& segment = segment_and_allow_reversal.first;
    const auto& allow_reversal = segment_and_allow_reversal.second;
    ls.push_back(point_type_fp(segment.low().x() / SCALE, segment.low().y() / SCALE));
    ls.push_back(point_type_fp(segment.high().x() / SCALE, segment.high().y() / SCALE));
    segments_as_linestrings.push_back(make_pair(ls, allow_reversal));
  }
  return segments_as_linestrings;
}

} //namespace segmentize
