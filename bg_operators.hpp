#ifndef BG_OPERATORS_HPP
#define BG_OPERATORS_HPP

#include "geometry.hpp"

#include <boost/functional/hash/hash.hpp>

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

template <typename polygon_type_t>
bg::model::multi_polygon<polygon_type_t> operator^(
    const bg::model::multi_polygon<polygon_type_t>& lhs,
    const bg::model::multi_polygon<polygon_type_t>& rhs);

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator&(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                   const rhs_t& rhs);

template <typename point_type_t, typename rhs_t>
multi_polygon_type_fp operator&(const bg::model::polygon<point_type_t>& lhs,
                                const rhs_t& rhs);

template <typename polygon_type_t, typename rhs_t>
bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                   const rhs_t& rhs);

multi_polygon_type_fp sum(const std::vector<multi_polygon_type_fp>& mpolys);
multi_polygon_type_fp symdiff(const std::vector<multi_polygon_type_fp>& mpolys);

// It's not great to insert definitions into the bg namespace but they
// are useful for sorting and maps.

namespace boost { namespace geometry { namespace model { namespace d2 {

template <typename T>
extern inline bool operator<(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y) {
  return std::tie(x.x(), x.y()) < std::tie(y.x(), y.y());
}

template <typename T>
extern inline boost::geometry::model::d2::point_xy<T> operator-(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const boost::geometry::model::d2::point_xy<T>& rhs) {
  return {lhs.x()-rhs.x(), lhs.y()-rhs.y()};
}

template <typename T>
extern inline boost::geometry::model::d2::point_xy<T> operator+(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const boost::geometry::model::d2::point_xy<T>& rhs) {
  return {lhs.x()+rhs.x(), lhs.y()+rhs.y()};
}

template <typename T, typename S>
extern inline boost::geometry::model::d2::point_xy<T> operator/(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const S& rhs) {
  return {lhs.x()/static_cast<T>(rhs), lhs.y()/static_cast<T>(rhs)};
}

template <typename T, typename S>
extern inline boost::geometry::model::d2::point_xy<T> operator*(
    const boost::geometry::model::d2::point_xy<T>& lhs,
    const S& rhs) {
  return {lhs.x()*static_cast<T>(rhs), lhs.y()*static_cast<T>(rhs)};
}

template <typename T>
extern inline bool operator==(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y) {
  return std::tie(x.x(), x.y()) == std::tie(y.x(), y.y());
}

template <typename T>
extern inline bool operator!=(
    const boost::geometry::model::d2::point_xy<T>& x,
    const boost::geometry::model::d2::point_xy<T>& y) {
  return std::tie(x.x(), x.y()) != std::tie(y.x(), y.y());
}

extern inline point_type_fp floor(const point_type_fp& a) {
  return point_type_fp(std::floor(a.x()), std::floor(a.y()));
}

template <typename T>
extern inline std::ostream& operator<<(std::ostream& out, const bg::model::d2::point_xy<T>& t) {
  out << bg::wkt(t);
  return out;
}

template <typename T>
extern inline std::ostream& operator<<(std::ostream& out, const bg::model::linestring<T>& t) {
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

template <typename T>
struct hash<boost::geometry::model::d2::point_xy<T>> {
  inline std::size_t operator()(const boost::geometry::model::d2::point_xy<T>& p) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, p.x());
    boost::hash_combine(seed, p.y());
    return seed;
  }
};

template <typename T0, typename T1>
struct hash<std::pair<T0, T1>> {
  inline std::size_t operator()(const std::pair<T0, T1>& x) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, hash<T0>{}(x.first));
    boost::hash_combine(seed, hash<T1>{}(x.second));
    return seed;
  }
};

template <typename T>
struct hash<boost::geometry::model::linestring<T>> {
  inline std::size_t operator()(const boost::geometry::model::linestring<T>& xs) const {
    std::size_t seed = 0;
    for (const auto& x : xs) {
      boost::hash_combine(seed, hash<T>{}(x));
    }
    return seed;
  }
};


} // namespace std

#endif //BG_OPERATORS_HPP
