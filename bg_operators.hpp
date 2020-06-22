#ifndef BG_OPERATORS_HPP
#define BG_OPERATORS_HPP

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
static std::ostream& operator<<(std::ostream& out, const bg::model::d2::point_xy<T>& t) {
  out << bg::wkt(t);
  return out;
}

template <typename T>
static std::ostream& operator<<(std::ostream& out, const bg::model::linestring<T>& t) {
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
  std::size_t operator()(const boost::geometry::model::d2::point_xy<T>& p) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, p.x());
    boost::hash_combine(seed, p.y());
    return seed;
  }
};

} // namespace std

#endif //BG_OPERATORS_HPP
