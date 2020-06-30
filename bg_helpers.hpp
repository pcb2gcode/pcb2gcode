#ifndef BG_HELPERS_H
#define BG_HELPERS_H

#include "eulerian_paths.hpp"
#ifdef GEOS_VERSION
#include <geos/io/WKTReader.h>
#include <geos/io/WKTWriter.h>
#include <geos/operation/buffer/BufferOp.h>
#endif // GEOS_VERSION

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator-(
    const bg::model::multi_polygon<polygon_type_t>& lhs,
    const rhs_t& rhs);

template <typename rhs_t>
multi_polygon_type_fp operator-(const polygon_type_fp& lhs, const rhs_t& rhs);

template <typename rhs_t>
multi_polygon_type_fp operator-(const box_type_fp& lhs, const rhs_t& rhs);

template <typename linestring_type_t, typename rhs_t>
bg::model::multi_linestring<linestring_type_t> operator-(
    const bg::model::multi_linestring<linestring_type_t>& lhs,
    const rhs_t& rhs);

template <typename linestring_type_t, typename rhs_t>
bg::model::multi_linestring<linestring_type_t> operator&(
    const bg::model::multi_linestring<linestring_type_t>& lhs,
    const rhs_t& rhs);

template <typename rhs_t>
multi_linestring_type_fp operator&(const linestring_type_fp& lhs,
                                   const rhs_t& rhs);

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator^(
    const bg::model::multi_polygon<polygon_type_t>& lhs,
    const rhs_t& rhs);

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator&(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                   const rhs_t& rhs);

template <typename point_type_t, typename rhs_t>
multi_polygon_type_fp operator&(const bg::model::polygon<point_type_t>& lhs,
                                const rhs_t& rhs);

//template <typename polygon_type_t, typename rhs_t>
//bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
//                                                   const rhs_t& rhs);

namespace bg_helpers {

// The below implementations of buffer are similar to bg::buffer but
// always convert to floating-point before doing work, if needed, and
// convert back afterward, if needed.  Also, they work if expand_by is
// 0, unlike bg::buffer.
template<typename CoordinateType>
void buffer(multi_polygon_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by);

multi_polygon_type_fp buffer(multi_polygon_type_fp const & geometry_in, coordinate_type_fp expand_by);

template<typename CoordinateType>
void buffer(polygon_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by);

template<typename CoordinateType>
void buffer(linestring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by);

template<typename CoordinateType>
multi_polygon_type_fp buffer(polygon_type_fp const & geometry_in, CoordinateType expand_by);

template<typename CoordinateType>
void buffer(multi_linestring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by);

template<typename CoordinateType>
multi_polygon_type_fp buffer(multi_linestring_type_fp const & geometry_in, CoordinateType expand_by);

template<typename CoordinateType>
void buffer(ring_type_fp const & geometry_in, multi_polygon_type_fp & geometry_out, CoordinateType expand_by);

} // namespace bg_helpers

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                   const rhs_t& rhs);

// It's not great to insert definitions into the bg namespace but they
// are useful for sorting and maps.

namespace boost { namespace geometry { namespace model { namespace d2 {

template <typename T>
bool operator<(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y);

template <typename T>
boost::geometry::model::d2::point_xy<T> operator-(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const boost::geometry::model::d2::point_xy<T>& rhs);

template <typename T>
boost::geometry::model::d2::point_xy<T> operator+(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const boost::geometry::model::d2::point_xy<T>& rhs);

template <typename T, typename S>
boost::geometry::model::d2::point_xy<T> operator/(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const S& rhs);

template <typename T, typename S>
boost::geometry::model::d2::point_xy<T> operator*(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const S& rhs);

template <typename T>
bool operator==(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y);

template <typename T>
bool operator!=(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y);

template <typename T>
static std::ostream& operator<<(std::ostream& out, const bg::model::linestring<T>& t) {
  out << bg::wkt(t);
  return out;
}

template <typename T>
static std::ostream& operator<<(std::ostream& out, const bg::model::d2::point_xy<T>& t) {
  out << bg::wkt(t);
  return out;
}

}}}} // namespace boost::geometry::model::d2

namespace std {

template <typename A, typename B>
static std::ostream& operator<<(std::ostream& out, const pair<A, B>& p) {
  out << "{" << p.first << "," << p.second << "}";
  return out;
}

template <typename T>
static std::ostream& operator<<(std::ostream& out, const vector<T>& xs) {
  out << "{";
  for (const auto& x : xs) {
    out << x << ",";
  }
  out << "}";
  return out;
}

} // namespace std

#endif //BG_HELPERS_H
