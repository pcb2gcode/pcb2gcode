#include "geometry.hpp"
#include "geometry_int.hpp"
#include "bg_helpers.hpp"

#include "bg_operators.hpp"

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator-(
    const bg::model::multi_polygon<polygon_type_t>& lhs,
    const rhs_t& rhs) {
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::difference(lhs, rhs, ret);
  return ret;
}

template multi_polygon_type_fp operator-(const multi_polygon_type_fp&, const multi_polygon_type_fp&);

template <> multi_polygon_type_fp operator-(const multi_polygon_type_fp& lhs, const ring_type_fp& rhs) {
  multi_polygon_type_fp rhs_mp{polygon_type_fp{rhs}};
  return lhs - rhs_mp;
}

template <typename rhs_t>
multi_polygon_type_fp operator-(const box_type_fp& lhs, const rhs_t& rhs) {
  auto box_mp = multi_polygon_type_fp();
  bg::convert(lhs, box_mp);
  return box_mp - rhs;
}

template multi_polygon_type_fp operator-(const box_type_fp&, const multi_polygon_type_fp&);

template <typename linestring_type_t, typename rhs_t>
bg::model::multi_linestring<linestring_type_t> operator-(
    const bg::model::multi_linestring<linestring_type_t>& lhs,
    const rhs_t& rhs) {
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  bg::model::multi_linestring<linestring_type_t> ret;
  if (bg::length(lhs) <= 0) {
    return ret;
  }
  bg::difference(lhs, rhs, ret);
  return ret;
}

template multi_linestring_type_fp operator-(const multi_linestring_type_fp&, const multi_polygon_type_fp&);

template <typename linestring_type_t, typename rhs_t>
bg::model::multi_linestring<linestring_type_t> operator&(
    const bg::model::multi_linestring<linestring_type_t>& lhs,
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

template multi_linestring_type_fp operator&(const multi_linestring_type_fp&, const multi_polygon_type_fp&);

template <>
multi_linestring_type_fp operator&(const multi_linestring_type_fp& lhs, const box_type_fp& rhs) {
  auto box_mp = multi_polygon_type_fp();
  bg::convert(rhs, box_mp);
  return lhs & box_mp;
}

template <typename rhs_t>
multi_linestring_type_fp operator&(const linestring_type_fp& lhs,
                                   const rhs_t& rhs) {
  return multi_linestring_type_fp{lhs} & rhs;
}

template multi_linestring_type_fp operator&(const linestring_type_fp&, const box_type_fp&);

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator&(const bg::model::multi_polygon<polygon_type_t>& lhs,
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

template multi_polygon_type_fp operator&(const multi_polygon_type_fp&, const multi_polygon_type_fp&);

template <>
multi_polygon_type_fp operator&(multi_polygon_type_fp const& lhs, polygon_type_fp const& rhs) {
  return lhs & multi_polygon_type_fp{rhs};
}

template <>
multi_polygon_type_fp operator&(multi_polygon_type_fp const& lhs, box_type_fp const& rhs) {
  auto box_mp = multi_polygon_type_fp();
  bg::convert(rhs, box_mp);
  return lhs & box_mp;
}


template <typename point_type_t, typename rhs_t>
multi_polygon_type_fp operator&(const bg::model::polygon<point_type_t>& lhs,
                                const rhs_t& rhs) {
  return multi_polygon_type_fp{lhs} & rhs;
}

template multi_polygon_type_fp operator&(polygon_type_fp const&, multi_polygon_type_fp const&);

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator^(
    const bg::model::multi_polygon<polygon_type_t>& lhs,
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

template multi_polygon_type_fp operator^(const multi_polygon_type_fp&, const multi_polygon_type_fp&);

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const rhs_t& rhs);

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
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

template multi_polygon_type_fp operator+(const multi_polygon_type_fp&, const multi_polygon_type_fp&);
template multi_polygon_type_fp operator+(const multi_polygon_type_fp&, const ring_type_fp&);

// It's not great to insert definitions into the bg namespace but they
// are useful for sorting and maps.

namespace boost { namespace geometry { namespace model { namespace d2 {

template <typename T>
bool operator<(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y) {
  return std::tie(x.x(), x.y()) < std::tie(y.x(), y.y());
}

template bool operator<(const point_type_fp&, const point_type_fp&);
template bool operator<(const ::point_type&, const ::point_type&);

template <typename T>
boost::geometry::model::d2::point_xy<T> operator-(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const boost::geometry::model::d2::point_xy<T>& rhs) {
  return {lhs.x()-rhs.x(), lhs.y()-rhs.y()};
}

template point_type_fp operator-(const point_type_fp&, const point_type_fp&);

template <typename T>
boost::geometry::model::d2::point_xy<T> operator+(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const boost::geometry::model::d2::point_xy<T>& rhs) {
  return {lhs.x()+rhs.x(), lhs.y()+rhs.y()};
}

template point_type_fp operator+(const point_type_fp&, const point_type_fp&);

template <typename T, typename S>
boost::geometry::model::d2::point_xy<T> operator/(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const S& rhs) {
  return {lhs.x()/static_cast<T>(rhs), lhs.y()/static_cast<T>(rhs)};
}

template point_type_fp operator/(const point_type_fp&, const double&);
template point_type_fp operator/(const point_type_fp&, const int&);

template <typename T, typename S>
boost::geometry::model::d2::point_xy<T> operator*(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const S& rhs) {
  return {lhs.x()*static_cast<T>(rhs), lhs.y()*static_cast<T>(rhs)};
}

template point_type_fp operator*(const point_type_fp&, const double&);

template <typename T>
bool operator==(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y) {
  return std::tie(x.x(), x.y()) == std::tie(y.x(), y.y());
}

template bool operator==<double>(const point_type_fp&, const point_type_fp&);

template <typename T>
bool operator!=(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y) {
  return std::tie(x.x(), x.y()) != std::tie(y.x(), y.y());
}

template bool operator!=<double>(const point_type_fp&, const point_type_fp&);

}}}} // namespace boost::geometry::model::d2
