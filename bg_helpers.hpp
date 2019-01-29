#ifndef BG_HELPERS_H
#define BG_HELPERS_H

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
    return multi_polygon_type_fp{lhs};
  }
  multi_polygon_type_fp ret;
  bg::difference(lhs, rhs, ret);
  return ret;
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

template <typename polygon_type_t, typename rhs_t>
static inline bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const rhs_t& rhs) {
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::union_(lhs, rhs, ret);
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

namespace bg_helpers {

// The below implementations of buffer are similar to bg::buffer but
// always convert to floating-point before doing work, if needed, and
// convert back afterward, if needed.  Also, they work if expand_by is
// 0, unlike bg::buffer.
static const int points_per_circle = 30;
template<typename CoordinateType>
void buffer(multi_polygon_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
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
void buffer(polygon_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
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
void buffer(polygon_type const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  if (expand_by == 0) {
    bg::convert(geometry_in, geometry_out);
  } else {
    polygon_type_fp geometry_in_fp;
    bg::convert(geometry_in, geometry_in_fp);
    buffer(geometry_in_fp, geometry_out, expand_by);
  }
}

template<typename CoordinateType>
void buffer(multi_linestring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  bg::buffer(geometry_in, geometry_out,
             bg::strategy::buffer::distance_symmetric<CoordinateType>(expand_by),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(points_per_circle),
             bg::strategy::buffer::end_round(points_per_circle),
             bg::strategy::buffer::point_circle(points_per_circle));
}

template<typename CoordinateType>
void buffer(ring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
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

#endif //BG_HELPERS_H
