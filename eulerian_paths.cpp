#include <boost/container/detail/std_fwd.hpp>  // for pair
#include <boost/geometry.hpp>                  // for linestring
#include <boost/polygon/point_data.hpp>        // for point_data
#include <boost/polygon/segment_data.hpp>      // for segment_data
#include <initializer_list>                    // for initializer_list
#include <memory>                              // for allocator_traits<>::va...
#include <utility>                             // for pair, make_pair
#include <vector>                              // for vector

#include "eulerian_paths.hpp"
#include "geometry.hpp"                        // for linestring_type_fp
#include "geometry_int.hpp"                    // for segment_type_p, point_...
#include "merge_near_points.hpp"               // for merge_near_points
#include "segmentize.hpp"                      // for segmentize

namespace eulerian_paths {

using std::vector;
using std::pair;

// For use when we have to convert from float to long and back.
const double SCALE = 1000000.0;

// Returns a minimal number of toolpaths that include all the milling in the
// oroginal toolpaths.  Each path is traversed once.  First paths are
// directional, second are bidi.  In the pair, the first is directional and the
// second is bidi.
multi_linestring_type_fp make_eulerian_paths(const vector<pair<linestring_type_fp, bool>>& toolpaths) {
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
  vector<std::pair<segment_type_p, bool>> split_segments = segmentize::segmentize(all_segments, allow_reversals);

  // Make a minimal number of paths from those segments.
  struct PointLessThan {
    bool operator()(const point_type_fp& a, const point_type_fp& b) const {
      return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
    }
  };
  // Only allow reversing the direction of travel if mill_feed_direction is
  // ANY.  We need to scale them back down.
  multi_linestring_type_fp segments_as_linestrings;
  segments_as_linestrings.reserve(split_segments.size());
  allow_reversals.clear();
  allow_reversals.reserve(split_segments.size());
  for (const auto& segment_and_allow_reversal : split_segments) {
    // Make a little 1-edge linestrings.
    linestring_type_fp ls;
    const auto& segment = segment_and_allow_reversal.first;
    const auto& allow_reversal = segment_and_allow_reversal.second;
    ls.push_back(point_type_fp(segment.low().x() / SCALE, segment.low().y() / SCALE));
    ls.push_back(point_type_fp(segment.high().x() / SCALE, segment.high().y() / SCALE));
    segments_as_linestrings.push_back(ls);
    allow_reversals.push_back(allow_reversal);
  }

  return get_eulerian_paths<
      point_type_fp,
      linestring_type_fp,
      multi_linestring_type_fp,
      PointLessThan>(segments_as_linestrings, allow_reversals);
}

multi_linestring_type_fp make_eulerian_paths(const std::vector<linestring_type_fp>& paths, bool reversible) {
  std::vector<std::pair<linestring_type_fp, bool>> path_to_simplify;
  for (const auto& ls : paths) {
    path_to_simplify.push_back(std::make_pair(ls, reversible));
  }
  return make_eulerian_paths(path_to_simplify);
}

} // namespace eulerian_paths
