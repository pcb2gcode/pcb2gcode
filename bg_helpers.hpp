#ifndef BG_HELPERS_H
#define BG_HELPERS_H

namespace bg_helpers {


// The below implementations of buffer are similar to bg::buffer but
// always convert to floating-point before doing work, if needed, and
// convert back afterward, if needed.  Also, they work if expand_by is
// 0, unlike bg::buffer.
static const int points_per_circle = 30;
template<typename CoordinateType>
void buffer(polygon_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  if (expand_by == 0) {
    bg::convert(geometry_in, geometry_out);
  } else {
    bg::buffer(geometry_in, geometry_out,
               bg::strategy::buffer::distance_symmetric<coordinate_type>(expand_by),
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
void buffer(multi_linestring_type const & geometry_in, multi_polygon_type & geometry_out, CoordinateType expand_by) {
  multi_linestring_type_fp geometry_in_fp;
  bg::convert(geometry_in, geometry_in_fp);
  multi_polygon_type_fp geometry_out_fp;
  bg::buffer(geometry_in_fp, geometry_out_fp,
             bg::strategy::buffer::distance_symmetric<coordinate_type>(expand_by),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(points_per_circle),
             bg::strategy::buffer::end_round(points_per_circle),
             bg::strategy::buffer::point_circle(points_per_circle));
  bg::convert(geometry_out_fp, geometry_out);
}

} // namespace bg_helpers

#endif //BG_HELPERS_H
