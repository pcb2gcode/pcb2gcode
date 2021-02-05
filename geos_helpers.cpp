#ifdef GEOS_VERSION

#include "geometry.hpp"
#include <geos/io/WKTReader.h>
#include <geos/io/WKTReader.inl>
#include <geos/io/WKTWriter.h>

template <typename T>
std::unique_ptr<geos::geom::Geometry> to_geos(const T& mp) {
  geos::io::WKTReader reader;
  std::stringstream ss;
  ss << bg::wkt(mp);
  return reader.read(ss.str());
}
template std::unique_ptr<geos::geom::Geometry> to_geos(const multi_polygon_type_fp& mp);
template std::unique_ptr<geos::geom::Geometry> to_geos(const multi_linestring_type_fp& mls);
template std::unique_ptr<geos::geom::Geometry> to_geos(const linestring_type_fp& mls);


template <typename T>
T from_geos(const std::unique_ptr<geos::geom::Geometry>& g);

template <>
multi_polygon_type_fp from_geos(const std::unique_ptr<geos::geom::Geometry>& g) {
  geos::io::WKTWriter writer;
  std::string geos_wkt = writer.write(g.get());
  boost::replace_all(geos_wkt, "EMPTY, ", "");
  boost::replace_all(geos_wkt, ", EMPTY", "");
  multi_polygon_type_fp ret;
  if (strncmp(geos_wkt.c_str(), "MULTIPOLYGON", 12) == 0) {
    bg::read_wkt(geos_wkt, ret);
  } else {
    polygon_type_fp poly;
    bg::read_wkt(geos_wkt, poly);
    ret.push_back(poly);
  }
  return ret;
}

template <>
multi_linestring_type_fp from_geos(const std::unique_ptr<geos::geom::Geometry>& g) {
  geos::io::WKTWriter writer;
  std::string geos_wkt = writer.write(g.get());
  boost::replace_all(geos_wkt, "EMPTY, ", "");
  boost::replace_all(geos_wkt, ", EMPTY", "");
  multi_linestring_type_fp ret;
  if (strncmp(geos_wkt.c_str(), "MULTILINESTRING", 12) == 0) {
    bg::read_wkt(geos_wkt, ret);
  } else {
    linestring_type_fp ls;
    bg::read_wkt(geos_wkt, ls);
    ret.push_back(ls);
  }
  return ret;
}

#endif //GEOS_VERSION
