#ifndef GEOMETRY_INT_H
#define GEOMETRY_INT_H

#include <vector>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/register/ring.hpp>
#include <boost/polygon/polygon.hpp>

typedef int64_t coordinate_type;
typedef boost::geometry::model::d2::point_xy<coordinate_type> point_type;
typedef boost::geometry::model::ring<point_type> ring_type;
typedef boost::geometry::model::box<point_type> box_type;
typedef boost::geometry::model::linestring<point_type> linestring_type;
typedef boost::geometry::model::multi_linestring<linestring_type> multi_linestring_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;
typedef boost::geometry::model::multi_polygon<polygon_type> multi_polygon_type;

typedef boost::polygon::point_data<coordinate_type> point_type_p;
typedef boost::polygon::segment_data<coordinate_type> segment_type_p;

#endif
