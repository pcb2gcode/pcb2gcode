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

multi_polygon_type_fp buffer_miter(multi_polygon_type_fp const & geometry_in, coordinate_type_fp expand_by) {
  if (expand_by == 0) {
    return geometry_in;
  } else {
    multi_polygon_type_fp geometry_out;
    bg::buffer(geometry_in, geometry_out,
               bg::strategy::buffer::distance_symmetric<coordinate_type_fp>(expand_by),
               bg::strategy::buffer::side_straight(),
               bg::strategy::buffer::join_miter(expand_by),
               bg::strategy::buffer::end_round(points_per_circle),
               bg::strategy::buffer::point_circle(points_per_circle));
    return geometry_out;
  }
}

template<typename CoordinateType>
multi_polygon_type_fp buffer(polygon_type_fp const & geometry_in, CoordinateType expand_by) {
  return buffer(multi_polygon_type_fp{geometry_in}, expand_by);
}

template multi_polygon_type_fp buffer(polygon_type_fp const&, double);

template<typename CoordinateType>
multi_polygon_type_fp buffer_miter(polygon_type_fp const & geometry_in, CoordinateType expand_by) {
  return buffer_miter(multi_polygon_type_fp{geometry_in}, expand_by);
}

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
multi_polygon_type_fp buffer(multi_linestring_type_fp const & geometry_in, CoordinateType expand_by) {
  if (expand_by == 0) {
    return {};
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
  multi_polygon_type_fp ret;
  if (strncmp(geos_wkt.c_str(), "MULTIPOLYGON", 12) == 0) {
    bg::read_wkt(geos_wkt, ret);
  } else {
    polygon_type_fp poly;
    bg::read_wkt(geos_wkt, poly);
    ret.push_back(poly);
  }
  return ret;
#else
  if (expand_by == 0) {
    return {};
  }
  multi_polygon_type_fp ret;
  for (const auto& ls : mls) {
    ret = ret + buffer(ls, expand_by);
  }
  return ret;
#endif
}

template multi_polygon_type_fp buffer(const multi_linestring_type_fp&, double expand_by);

template<typename CoordinateType>
multi_polygon_type_fp buffer_miter(ring_type_fp const & geometry_in, CoordinateType expand_by) {
  return buffer_miter(polygon_type_fp{geometry_in}, expand_by);
}

template multi_polygon_type_fp buffer_miter(ring_type_fp const&, double);

} // namespace bg_helpers
