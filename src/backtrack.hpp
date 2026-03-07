#ifndef BACKTRACK_HPP
#define BACKTRACK_HPP

namespace backtrack {

// Find paths in the input that, if doubled so that they could be
// traversed twice, would decrease the milling time overall.  The
// input is a list of paths and the reversibility of each one.  The is
// just the paths that need to be reversed and added.  in_per_sec is
// the number of inches of unnecessary milling that the user is
// willing to do in order to save seconds.
std::vector<std::pair<linestring_type_fp, bool>> backtrack(
    const std::vector<std::pair<linestring_type_fp, bool>>& paths,
    const double g1_speed, const double up_time, const double g0_speed, const double down_time,
    const double mm_per_second);

} // namespace backtrack
#endif //BACKTRACK_HPP
