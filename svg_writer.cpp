#include <string>
#include <boost/format.hpp>
#include <memory>
#include "geometry.hpp"
#include "bg_operators.hpp"
#include "svg_writer.hpp"

using std::string;
using std::unique_ptr;
using std::make_unique;

svg_writer::svg_writer(string filename, box_type_fp bounding_box) :
    output_file(filename),
    bounding_box(bounding_box)
{
    const coordinate_type_fp width =
        (bounding_box.max_corner().x() - bounding_box.min_corner().x()) * SVG_PIX_PER_IN;
    const coordinate_type_fp height =
        (bounding_box.max_corner().y() - bounding_box.min_corner().y()) * SVG_PIX_PER_IN;
    const coordinate_type_fp viewBox_width =
        (bounding_box.max_corner().x() - bounding_box.min_corner().x()) * SVG_DOTS_PER_IN;
    const coordinate_type_fp viewBox_height =
        (bounding_box.max_corner().y() - bounding_box.min_corner().y()) * SVG_DOTS_PER_IN;

    //Some SVG readers does not behave well when viewBox is not specified
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"0 0 %3% %4%\"") % width % height % viewBox_width % viewBox_height);

    mapper = make_unique<bg::svg_mapper<point_type_fp>>(
        output_file, viewBox_width, viewBox_height, svg_dimensions);
    mapper->add(bounding_box);
}

template <typename multi_polygon_type_t>
void svg_writer::add(const multi_polygon_type_t& geometry, double opacity, bool stroke)
{
    string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:2" : "";

    for (const auto& poly : geometry)
    {
        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;

        multi_polygon_type_t new_bounding_box;
        bg::convert(bounding_box, new_bounding_box);

        mapper->map(poly & new_bounding_box,
            str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);" + stroke_str) %
            opacity % r % g % b));
    }
}

template void svg_writer::add<multi_polygon_type_fp>(const multi_polygon_type_fp&, double, bool);

void svg_writer::add(const multi_linestring_type_fp& mls, coordinate_type_fp width, bool stroke) {
  string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:2" : "";

  for (const auto& ls : mls) {
    const unsigned int r = rand() % 256;
    const unsigned int g = rand() % 256;
    const unsigned int b = rand() % 256;

    add(ls, width, r, g, b);
  }
}

void svg_writer::add(const linestring_type_fp& path, coordinate_type_fp width, unsigned int r, unsigned int g, unsigned int b) {
  // Stroke the width of the path.
  mapper->map(path,
              str(boost::format("stroke:rgb(%u,%u,%u);stroke-width:%f;fill:none;"
                                "stroke-opacity:0.5;stroke-linecap:round;stroke-linejoin:round;") % r % g % b % (width * SVG_DOTS_PER_IN)));
  // Stroke the center of the path.
  mapper->map(path,
              "stroke:rgb(0,0,0);stroke-width:1px;fill:none;"
              "stroke-opacity:1;stroke-linecap:round;stroke-linejoin:round;");
}

void svg_writer::add(const multi_linestring_type_fp& path, coordinate_type_fp width, unsigned int r, unsigned int g, unsigned int b) {
  for (const auto& p : path) {
    add(p, width, r, g, b);
  }
}
