#ifndef BG_HELPERS_H
#define BG_HELPERS_H

#include "eulerian_paths.hpp"

template <typename polygon_type_t, typename rhs_t>
static inline bg::model::multi_polygon<polygon_type_t> operator-(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const rhs_t& rhs) {
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::difference(lhs, rhs, ret);
  return ret;
}

template <typename rhs_t>
static inline multi_polygon_type_fp operator-(const polygon_type_fp& lhs, const rhs_t& rhs) {
  if (bg::area(rhs) <= 0) {
    auto ret = multi_polygon_type_fp();
    ret.push_back(lhs);
    return ret;
  }
  multi_polygon_type_fp ret;
  bg::difference(lhs, rhs, ret);
  return ret;
}

template <typename rhs_t>
static inline multi_polygon_type_fp operator-(const box_type_fp& lhs, const rhs_t& rhs) {
  auto box_mp = multi_polygon_type_fp();
  bg::convert(lhs, box_mp);
  return box_mp - rhs;
}

template <typename linestring_type_t, typename rhs_t>
static inline bg::model::multi_linestring<linestring_type_t> operator-(const bg::model::multi_linestring<linestring_type_t>& lhs,
                                                                       const rhs_t& rhs) {
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  bg::model::multi_linestring<linestring_type_t> ret;
  bg::difference(lhs, rhs, ret);
  return ret;
}

template <typename linestring_type_t, typename rhs_t>
static inline bg::model::multi_linestring<linestring_type_t> operator&(const bg::model::multi_linestring<linestring_type_t>& lhs,
                                                                       const rhs_t& rhs) {
  bg::model::multi_linestring<linestring_type_t> ret;
  if (bg::area(rhs) <= 0) {
    return ret;
  }
  if (bg::length(lhs) <= 0) {
    return ret;
  }
  bg::intersection(lhs, rhs, ret);
  return ret;
}

template <typename rhs_t>
static inline multi_linestring_type_fp operator&(const linestring_type_fp& lhs,
                                                 const rhs_t& rhs) {
  multi_linestring_type_fp ret;
  if (bg::area(rhs) <= 0) {
    return ret;
  }
  if (bg::length(lhs) <= 0) {
    return ret;
  }
  bg::intersection(lhs, rhs, ret);
  return ret;
}

template <typename polygon_type_t, typename rhs_t>
static inline bg::model::multi_polygon<polygon_type_t> operator^(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const rhs_t& rhs) {
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  if (bg::area(lhs) <= 0) {
    return rhs;
  }
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::sym_difference(lhs, rhs, ret);
  return ret;
}

template <typename polygon_type_t, typename rhs_t>
static inline bg::model::multi_polygon<polygon_type_t> operator&(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const rhs_t& rhs) {
  bg::model::multi_polygon<polygon_type_t> ret;
  if (bg::area(rhs) <= 0) {
    return ret;
  }
  if (bg::area(lhs) <= 0) {
    return ret;
  }
  bg::intersection(lhs, rhs, ret);
  return ret;
}

template <typename point_type_t, typename rhs_t>
static inline multi_polygon_type_fp operator&(const bg::model::polygon<point_type_t>& lhs,
                                              const rhs_t& rhs) {
  multi_polygon_type_fp ret;
  if (bg::area(rhs) <= 0) {
    return ret;
  }
  if (bg::area(lhs) <= 0) {
    return ret;
  }
  bg::intersection(lhs, rhs, ret);
  return ret;
}

template <typename polygon_type_t, typename rhs_t>
static inline bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const rhs_t& rhs);
namespace bg_helpers {

// The below implementations of buffer are similar to bg::buffer but
// always convert to floating-point before doing work, if needed, and
// convert back afterward, if needed.  Also, they work if expand_by is
// 0, unlike bg::buffer.
static const int points_per_circle = 30;
template<typename CoordinateType>
static inline void buffer(multi_polygon_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  if (expand_by == 0) {
    bg::convert(geometry_in, geometry_out);
  } else {
    bg::buffer(geometry_in, geometry_out,
               bg::strategy::buffer::distance_symmetric<CoordinateType>(expand_by),
               bg::strategy::buffer::side_straight(),
               bg::strategy::buffer::join_round(points_per_circle),
               bg::strategy::buffer::end_round(points_per_circle),
               bg::strategy::buffer::point_circle(points_per_circle));
  }
}

static inline multi_polygon_type_fp buffer(multi_polygon_type_fp const & geometry_in, coordinate_type_fp expand_by) {
  if (expand_by == 0) {
    return geometry_in;
  } else {
    multi_polygon_type_fp geometry_out;
    bg::buffer(geometry_in, geometry_out,
               bg::strategy::buffer::distance_symmetric<coordinate_type_fp>(expand_by),
               bg::strategy::buffer::side_straight(),
               bg::strategy::buffer::join_round(points_per_circle),
               bg::strategy::buffer::end_round(points_per_circle),
               bg::strategy::buffer::point_circle(points_per_circle));
    return geometry_out;
  }
}

template<typename CoordinateType>
static inline void buffer(polygon_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  if (expand_by == 0) {
    bg::convert(geometry_in, geometry_out);
  } else {
    bg::buffer(geometry_in, geometry_out,
               bg::strategy::buffer::distance_symmetric<CoordinateType>(expand_by),
               bg::strategy::buffer::side_straight(),
               bg::strategy::buffer::join_round(points_per_circle),
               bg::strategy::buffer::end_round(points_per_circle),
               bg::strategy::buffer::point_circle(points_per_circle));
  }
}

template<typename CoordinateType>
static inline void buffer(linestring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  if (expand_by == 0) {
    geometry_out.clear();
  } else {
    bg::buffer(geometry_in, geometry_out,
               bg::strategy::buffer::distance_symmetric<CoordinateType>(expand_by),
               bg::strategy::buffer::side_straight(),
               bg::strategy::buffer::join_round(points_per_circle),
               bg::strategy::buffer::end_round(points_per_circle),
               bg::strategy::buffer::point_circle(points_per_circle));
  }
}

template<typename CoordinateType>
static inline multi_polygon_type_fp buffer(polygon_type_fp const & geometry_in, CoordinateType expand_by) {
  multi_polygon_type_fp geometry_out;
  buffer(geometry_in, geometry_out, expand_by);
  return geometry_out;
}

template<typename CoordinateType>
static inline void buffer(multi_linestring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  if (expand_by == 0) {
    geometry_out.clear();
    return;
  }
  // bg::buffer of multilinestring is broken in boost.  Converting the
  // multilinestring to non-intersecting paths seems to help.
  multi_linestring_type_fp mls = eulerian_paths::make_eulerian_paths(geometry_in, true);
  geometry_out.clear();
  if (expand_by == 0) {
    return;
  }
  for (const auto& ls : mls) {
    multi_polygon_type_fp buf;
    buffer(ls, buf, expand_by);
    geometry_out = geometry_out + buf;
  }
}

template<typename CoordinateType>
static inline multi_polygon_type_fp buffer(multi_linestring_type_fp const & geometry_in, CoordinateType expand_by) {
  multi_polygon_type_fp geometry_out;
  buffer(geometry_in, geometry_out, expand_by);
  return geometry_out;
}

template<typename CoordinateType>
static inline void buffer(ring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  if (expand_by == 0) {
    bg::convert(geometry_in, geometry_out);
  } else {
    polygon_type_fp geometry_in_fp;
    bg::convert(geometry_in, geometry_in_fp);
    multi_polygon_type_fp geometry_out_fp;
    bg::buffer(geometry_in_fp, geometry_out,
               bg::strategy::buffer::distance_symmetric<CoordinateType>(expand_by),
               bg::strategy::buffer::side_straight(),
               bg::strategy::buffer::join_round(),
               bg::strategy::buffer::end_round(),
               bg::strategy::buffer::point_circle());
  }
}
} // namespace bg_helpers

template <typename polygon_type_t, typename rhs_t>
static inline bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const rhs_t& rhs) {
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  if (bg::area(lhs) <= 0) {
    bg::model::multi_polygon<polygon_type_t> ret;
    bg::convert(rhs, ret);
    return ret;
  }
  // This optimization fixes a bug in boost geometry when shapes are bordering
  // somwhat but not overlapping.  This is exposed by EasyEDA that makes lots of
  // shapes like that.
  const auto lhs_box = bg::return_envelope<box_type_fp>(lhs);
  const auto rhs_box = bg::return_envelope<box_type_fp>(rhs);
  if (lhs_box.max_corner().x() == rhs_box.min_corner().x() ||
      rhs_box.max_corner().x() == lhs_box.min_corner().x() ||
      lhs_box.max_corner().y() == rhs_box.min_corner().y() ||
      rhs_box.max_corner().y() == lhs_box.min_corner().y()) {
    multi_polygon_type_fp new_rhs;
    bg::convert(rhs, new_rhs);
    return bg_helpers::buffer(lhs, 0.00001) + bg_helpers::buffer(new_rhs, 0.00001);
  }
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::union_(lhs, rhs, ret);
  return ret;
}

#endif //BG_HELPERS_H
