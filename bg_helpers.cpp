#include "eulerian_paths.hpp"
#ifdef GEOS_VERSION
#include <geos/io/WKTReader.h>
#include <geos/io/WKTWriter.h>
#include <geos/operation/buffer/BufferOp.h>
#endif // GEOS_VERSION

#include "bg_operators.hpp"
#include "bg_helpers.hpp"

namespace bg_helpers {

// The below implementations of buffer are similar to bg::buffer but
// always convert to floating-point before doing work, if needed, and
// convert back afterward, if needed.  Also, they work if expand_by is
// 0, unlike bg::buffer.
const int points_per_circle = 32;

multi_polygon_type_fp buffer(multi_polygon_type_fp const & geometry_in, coordinate_type_fp expand_by) {
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

template void buffer(const polygon_type_fp&, multi_polygon_type_fp&, double);

template<typename CoordinateType>
void buffer(linestring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
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
multi_polygon_type_fp buffer(polygon_type_fp const & geometry_in, CoordinateType expand_by) {
  multi_polygon_type_fp geometry_out;
  buffer(geometry_in, geometry_out, expand_by);
  return geometry_out;
}

template multi_polygon_type_fp buffer(polygon_type_fp const&, double);

template<typename CoordinateType>
multi_polygon_type_fp buffer(linestring_type_fp const & geometry_in, CoordinateType expand_by) {
  if (expand_by == 0) {
    return {};
  }
  multi_polygon_type_fp geometry_out;
  bg::buffer(geometry_in, geometry_out,
             bg::strategy::buffer::distance_symmetric<coordinate_type_fp>(expand_by),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(points_per_circle),
             bg::strategy::buffer::end_round(points_per_circle),
             bg::strategy::buffer::point_circle(points_per_circle));
  return geometry_out;
}

template<typename CoordinateType>
void buffer(multi_linestring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by) {
  if (expand_by == 0) {
    geometry_out.clear();
    return;
  }
  // bg::buffer of multilinestring is broken in boost.  Converting the
  // multilinestring to non-intersecting paths seems to help.
  multi_linestring_type_fp mls = eulerian_paths::make_eulerian_paths(geometry_in, true, true);
#ifdef GEOS_VERSION
  geos::io::WKTReader reader;
  std::stringstream ss;
  ss << bg::wkt(mls);
  auto geos_in = reader.read(ss.str());
  std::unique_ptr<geos::geom::Geometry> geos_out(geos::operation::buffer::BufferOp::bufferOp(geos_in.get(), expand_by, points_per_circle/4));
  geos::io::WKTWriter writer;
  auto geos_wkt = writer.write(geos_out.get());
  if (strncmp(geos_wkt.c_str(), "MULTIPOLYGON", 12) == 0) {
    bg::read_wkt(geos_wkt, geometry_out);
  } else {
    polygon_type_fp poly;
    bg::read_wkt(geos_wkt, poly);
    geometry_out.clear();
    geometry_out.push_back(poly);
  }
#else
  geometry_out.clear();
  if (expand_by == 0) {
    return;
  }
  for (const auto& ls : mls) {
    multi_polygon_type_fp buf;
    buffer(ls, buf, expand_by);
    geometry_out = geometry_out + buf;
  }
#endif
}

template void buffer(const multi_linestring_type_fp&, multi_polygon_type_fp&, double expand_by);

template<typename CoordinateType>
multi_polygon_type_fp buffer(multi_linestring_type_fp const & geometry_in, CoordinateType expand_by) {
  multi_polygon_type_fp geometry_out;
  buffer(geometry_in, geometry_out, expand_by);
  return geometry_out;
}

template multi_polygon_type_fp buffer(const multi_linestring_type_fp&, double expand_by);

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

template void buffer(ring_type_fp const&, multi_polygon_type_fp&, double);

} // namespace bg_helpers
