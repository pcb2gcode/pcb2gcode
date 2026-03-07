#ifndef MERGE_NEAR_POINTS_HPP
#define MERGE_NEAR_POINTS_HPP

#include <vector>
#include <utility>

size_t merge_near_points(std::vector<std::pair<linestring_type_fp, bool>>& mls, const coordinate_type_fp distance);
size_t merge_near_points(multi_linestring_type_fp& mls, const coordinate_type_fp distance);

#endif //MERGE_NEAR_POINTS_HPP
