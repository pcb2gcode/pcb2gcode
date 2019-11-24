#include <iostream>                                 // for ios_base::failure
#include <string>                                   // for string

#include "geometry.hpp"                             // for multi_linestring_...

using std::string;

#include <bits/exception.h>                         // for exception
#include <boost/detail/basic_pointerbuf.hpp>        // for basic_pointerbuf<...
#include <boost/format.hpp>                         // for basic_altstringbu...
#include <boost/geometry.hpp>                       // for read_wkt, svg_mapper
#include <boost/lexical_cast/bad_lexical_cast.hpp>  // for bad_lexical_cast
#include <string.h>                                 // for strncmp

int main() {
  box_type_fp bounding_box;
  bg::envelope(point_type_fp(0,0), bounding_box);
  bg::expand(bounding_box, point_type_fp(14,14));
  const coordinate_type_fp viewBox_width =
      (bounding_box.max_corner().x() - bounding_box.min_corner().x()) * SVG_DOTS_PER_IN;
  const coordinate_type_fp viewBox_height =
      (bounding_box.max_corner().y() - bounding_box.min_corner().y()) * SVG_DOTS_PER_IN;

  //Some SVG readers does not behave well when viewBox is not specified
  const string svg_dimensions =
      str(boost::format("viewBox=\"0 0 %1% %2%\"") % viewBox_width % viewBox_height);

  bg::svg_mapper<point_type_fp> mapper(std::cout, viewBox_width, viewBox_height, svg_dimensions);
  char buffer[1024*64];
  std::cin.getline(buffer, 1024*64);
  while (buffer[0] != 0) {
    if (strncmp(buffer, "MULTILINESTRING", 15) == 0) {
      multi_linestring_type_fp mls;
      bg::read_wkt(buffer, mls);
      mapper.add(mls);
      mapper.map(mls,
                 "stroke:rgb(0,0,0);stroke-width:10;fill:none;"
                 "stroke-opacity:0.3;stroke-linecap:round;stroke-linejoin:round;");
    } else if (strncmp(buffer, "LINESTRING", 10) == 0) {
      linestring_type_fp ls;
      bg::read_wkt(buffer, ls);
      mapper.add(ls);
      mapper.map(ls,
                 "stroke:rgb(0,0,0);stroke-width:10;fill:none;"
                 "stroke-opacity:0.3;stroke-linecap:round;stroke-linejoin:round;");
    } else {
      multi_polygon_type_fp mp;
      bg::read_wkt(buffer, mp);
      mapper.add(mp);
      mapper.map(mp,
                 "stroke:rgb(0,0,0);stroke-width:10;fill:red;"
                 "stroke-opacity:1;stroke-linecap:round;stroke-linejoin:round;");
    }
    std::cin.getline(buffer, 1024*64);
  }
}
