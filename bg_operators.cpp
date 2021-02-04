#include "geometry.hpp"
#include "geometry_int.hpp"
#include "bg_helpers.hpp"
#ifdef GEOS_VERSION
#include <geos/operation/union/CascadedUnion.h>
#include <geos/util/TopologyException.h>
#include "geos_helpers.hpp"
#endif // GEOS_VERSION

#include "bg_operators.hpp"

using std::unique_ptr;
using std::vector;

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

template <typename polygon_type_t>
bg::model::multi_polygon<polygon_type_t> operator^(
    const bg::model::multi_polygon<polygon_type_t>& lhs,
    const bg::model::multi_polygon<polygon_type_t>& rhs) {
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

template <typename Addition>
multi_polygon_type_fp reduce(const std::vector<multi_polygon_type_fp>& mpolys,
                          const Addition& adder,
                          const std::vector<box_type_fp>& bboxes) {
  if (mpolys.size() == 0) {
    return multi_polygon_type_fp();
  } else if (mpolys.size() == 1) {
    return mpolys.front();
  }
  size_t current = 0;
  std::vector<multi_polygon_type_fp> new_mpolys;
  std::vector<box_type_fp> new_bboxes;
  if (mpolys.size() % 2 == 1) {
    new_mpolys.push_back(mpolys[current]);
    new_bboxes.push_back(bboxes[current]);
    current++;
  }
  // There are at least two and the total number is even.
  for (; current < mpolys.size(); current += 2) {
    box_type_fp new_bbox = bboxes[current];
    bg::expand(new_bbox, bboxes[current+1]);
    new_bboxes.push_back(new_bbox);
    if (!bg::intersects(bboxes[current], bboxes[current+1])) {
      new_mpolys.push_back(mpolys[current]);
      new_mpolys.back().insert(new_mpolys.back().cend(), mpolys[current+1].cbegin(), mpolys[current+1].cend());
    } else {
      new_mpolys.push_back(adder(mpolys[current], mpolys[current+1]));
    }
  }
  return reduce(new_mpolys, adder, new_bboxes);
}

template <typename Addition>
multi_polygon_type_fp reduce(const std::vector<multi_polygon_type_fp>& mpolys,
                             const Addition& adder) {
  std::vector<box_type_fp> bboxes;
  bboxes.reserve(mpolys.size());
  for (const auto& mpoly : mpolys) {
    bboxes.push_back(bg::return_envelope<box_type_fp>(mpoly));
  }
  return reduce(mpolys, adder, bboxes);
}

multi_polygon_type_fp sum(const std::vector<multi_polygon_type_fp>& mpolys) {
  if (mpolys.size() == 0) {
    return multi_polygon_type_fp();
  } else if (mpolys.size() == 1) {
    return mpolys[0];
  }
#ifdef GEOS_VERSION
  std::vector<geos::geom::Geometry*> geos_mpolys;
  std::vector<std::unique_ptr<geos::geom::Geometry>> geos_mpolys_tmp;
  for (const auto& mpoly : mpolys) {
    if (bg::area(mpoly) == 0) {
      continue;
    }
    geos_mpolys_tmp.push_back(to_geos(mpoly));
    geos_mpolys.push_back(geos_mpolys_tmp.back().get());
  }
  try {
    std::unique_ptr<geos::geom::Geometry> geos_out(
        geos::operation::geounion::CascadedUnion::Union(&geos_mpolys));
    return from_geos<multi_polygon_type_fp>(geos_out);
  } catch (const geos::util::TopologyException& e) {
    std::cerr << "\nError: Internal error with libgeos.  Upgrading geos may help." << std::endl;
    throw;
  }
#else // !GEOS_VERSION
  return reduce(mpolys, operator+<polygon_type_fp, multi_polygon_type_fp>);
#endif // GEOS_VERSION
}

multi_polygon_type_fp symdiff(const std::vector<multi_polygon_type_fp>& mpolys) {
  if (mpolys.size() == 0) {
    return multi_polygon_type_fp();
  } else if (mpolys.size() == 1) {
    return mpolys[0];
  }
  std::vector<box_type_fp> bboxes;
  bboxes.reserve(mpolys.size());
  for (const auto& mpoly : mpolys) {
    bboxes.push_back(bg::return_envelope<box_type_fp>(mpoly));
  }
  return reduce(mpolys, operator^<polygon_type_fp>);
}
