#ifndef SEGMENTIZE_H
#define SEGMENTIZE_H

#include <vector>
#include <map>

#include "geometry_int.hpp"
#include <boost/polygon/isotropy.hpp>
#include <boost/polygon/segment_concept.hpp>
#include <boost/polygon/segment_utils.hpp>

namespace segmentize {

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
std::vector<std::pair<segment_type_p, bool>> segmentize(const std::vector<segment_type_p>& all_segments,
                                                        const std::vector<bool>& allow_reversals);
} //namespace segmentize
#endif //SEGMENTIZE_H
