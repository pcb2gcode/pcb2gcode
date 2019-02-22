#define BOOST_TEST_MODULE gerberimporter tests
#include <boost/test/included/unit_test.hpp>

#include "gerberimporter.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <glibmm/init.h>
#include <gdkmm/wrap_init.h>
#include <sstream>
#include <librsvg-2.0/librsvg/rsvg.h>
#include <boost/format.hpp>
#include <cstdlib>
#include <string>
using std::string;

#include <map>
using std::map;

struct Fixture {
  Fixture() {
    Glib::init();
    Gdk::wrap_init();
  }
  ~Fixture() {}
};

BOOST_FIXTURE_TEST_SUITE(gerberimporter_tests, Fixture);

const unsigned int dpi = 1000;
const uint32_t BACKGROUND_COLOR = 0x00000000; // empty canvas starting color: black
const uint32_t NEW_BACKGROUND_COLOR = 0xff000000; // black
const uint32_t OLD_COLOR = 0xff0000ff; // blue
const uint32_t NEW_COLOR = 0x40ff0000; // red
const uint32_t NEW_BOTH_COLOR = 0xff002000; // The color that the both color is changed to.

// Make a surface of the right size for the input gerber.  Cairo already sets all the pixels to BACKGROUND_COLOR (0).
Cairo::RefPtr<Cairo::ImageSurface> create_cairo_surface(double width, double height) {
  return Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32,
                                     width,
                                     height);
}

// Given a gerber file, return a pixmap that is a rasterized version of that
// gerber.  Uses gerbv's built-in utils.
void bitmap_from_gerber(const GerberImporter& g, double min_x, double min_y, double width, double height,
                        Cairo::RefPtr<Cairo::ImageSurface> cairo_surface) {
  //Render
  GdkColor blue = {0,
                   ((OLD_COLOR >> 16) & 0xff) * 0x101,
                   ((OLD_COLOR >>  8) & 0xff) * 0x101,
                   ((OLD_COLOR      ) & 0xff) * 0x101};
  g.render(cairo_surface, dpi, min_x, min_y, blue, GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY);
}

string render_svg(const multi_polygon_type_fp& polys, double min_x, double min_y, double width, double height) {
  std::stringstream svg_stream;
  {
    box_type_fp svg_bounding_box;
    bg::envelope(polys, svg_bounding_box);
    const double svg_width = (svg_bounding_box.max_corner().x() - svg_bounding_box.min_corner().x());
    const double svg_height = (svg_bounding_box.max_corner().y() - svg_bounding_box.min_corner().y());
    const double max_y = min_y + height;
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"%3% %4% %5% %6%\"")
            % "100%"
            % "100%"
            % ((min_x - svg_bounding_box.min_corner().x()) * dpi)
            // max and not min because the origin is in a different corner for svg vs gbr
            % ((svg_bounding_box.max_corner().y() - max_y) * dpi)
            % (svg_width * dpi)
            % (svg_height * dpi));
    bg::svg_mapper<point_type_fp> svg(svg_stream,
                                      svg_width * dpi,
                                      svg_height * dpi,
                                      svg_dimensions);
    svg.add(polys); // This is needed for the next line to work, not sure why.
    svg.map(polys, str(boost::format("fill-opacity:%1%;fill:rgb(%2%,%3%,%4%);")
                       % (((NEW_COLOR >> 24) & 0xff ) / double(0xff))
                       % ((NEW_COLOR >> 16) & 0xff)
                       % ((NEW_COLOR >>  8) & 0xff)
                       % ((NEW_COLOR      ) & 0xff)));
  } // The svg file is complete when it goes out of scope.
  return svg_stream.str();
}

// Convert the gerber to a boost geometry and then convert that to SVG and then rasterize to a bitmap.
void boost_bitmap_from_gerber(const multi_polygon_type_fp& polys, double min_x, double min_y, double width, double height,
                              Cairo::RefPtr<Cairo::ImageSurface> cairo_surface) {
  string svg_string = render_svg(polys, min_x, min_y, width, height);
  //Now we have the svg, let's make a cairo surface like above.
  GError* gerror = nullptr;
  RsvgHandle *rsvg_handle = rsvg_handle_new_from_data(reinterpret_cast<const guint8*>(svg_string.c_str()),
                                                      svg_string.size(),
                                                      &gerror);
  Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(cairo_surface);
  //cr->set_operator(Cairo::Operator::OPERATOR_XOR);
  BOOST_REQUIRE(rsvg_handle_render_cairo(rsvg_handle, cr->cobj()));
}

const string gerber_directory = "testing/gerberimporter";

map<uint32_t, size_t> get_counts(const Cairo::RefPtr<Cairo::ImageSurface>& cairo_surface) {
  size_t background = 0, both = 0, unknown = 0;
  // We only care about a few colors: background, match, red, blue, and all the rest.
  guint8* pixels = cairo_surface->get_data();
  int stride = cairo_surface->get_stride();
  auto height = cairo_surface->get_height();
  auto width = cairo_surface->get_width();
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      auto *current_color = reinterpret_cast<uint32_t *>(pixels + x*4 + y*stride);
      if (*current_color == BACKGROUND_COLOR) {
        *current_color = NEW_BACKGROUND_COLOR;
        background++;
      } else if ((*current_color & 0xff) &&
                 (*current_color & 0xff0000)) {
        both++;
        // Make it stick out less so that we can better see errors.
        *current_color = NEW_BOTH_COLOR;
      } else if (*current_color & 0xff) {
        *current_color = 0xff0000ff;
        unknown++;
      } else if (*current_color & 0xff0000) {
        *current_color = 0xffff0000;
        unknown++;
      }
    }
  }
  map<uint32_t, size_t> counts;
  counts[NEW_BACKGROUND_COLOR] = background;
  counts[NEW_BOTH_COLOR] = both;
  counts[0x12345678] = unknown;
  return counts;
}

void write_to_png(Cairo::RefPtr<Cairo::ImageSurface> cairo_surface, const string& gerber_file) {
  const char *skip_png = std::getenv("SKIP_GERBERIMPORTER_TESTS_PNG");
  if (skip_png != nullptr) {
    std::cout << "Skipping png generation because SKIP_GERBERIMPORTER_TESTS_PNG is set in environment." << std::endl;
    return;
  }
  cairo_surface->write_to_png(str(boost::format("%s.png") % gerber_file).c_str());
}

// Compare gerbv image against boost generated image.
void test_one(const string& gerber_file, double max_error_rate) {
  string gerber_path = gerber_directory;
  gerber_path += "/";
  gerber_path += gerber_file;
  auto g = GerberImporter();
  BOOST_REQUIRE(g.load_file(gerber_path));
  multi_polygon_type_fp polys = g.render(false, 360);
  box_type_fp bounding_box;
  bg::envelope(polys, bounding_box);
  double min_x = std::min(g.get_min_x(), bounding_box.min_corner().x());
  double min_y = std::min(g.get_min_y(), bounding_box.min_corner().y());
  double max_x = std::max(g.get_max_x(), bounding_box.max_corner().x());
  double max_y = std::max(g.get_max_y(), bounding_box.max_corner().y());
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface = create_cairo_surface((max_x - min_x) * dpi, (max_y - min_y) * dpi);
  bitmap_from_gerber(g, min_x, min_y, max_x-min_x, max_y-min_y, cairo_surface);
  boost_bitmap_from_gerber(polys, min_x, min_y, max_x-min_x, max_y-min_y, cairo_surface);
  map<uint32_t, size_t> counts = get_counts(cairo_surface);
  size_t background = 0, errors = 0, both = 0;
  for (const auto& kv : counts) {
    switch (kv.first) {
      case NEW_BACKGROUND_COLOR:
        background += kv.second;
        break;
      case NEW_BOTH_COLOR:
        both += kv.second;
        break;
      default:
        errors += kv.second;
        break;
    }
  }
  unsigned int total = errors + both;
  double error_rate = double(errors)/total;
  auto old_precision = std::cout.precision(3);
  auto old_width = std::cout.width(40);
  std::cout << gerber_file;
  std::cout.width(old_width);
  std::cout << "\t error rate: " << double(errors)/total*100 << "%"
            << "\t expected less than: " << max_error_rate*100 << "%"
            << std::endl;
  std::cout.precision(old_precision);
  BOOST_CHECK_LE(error_rate, max_error_rate);
  write_to_png(cairo_surface, gerber_file);
}

// For cases when even gerbv is wrong, just check that the number of pixels
// marked is more or less correct.  Look at http://www.gerber-viewer.com/ to
// test these.
void test_visual(const string& gerber_file, bool fill_closed_lines, double min_set_ratio, double max_set_ratio) {
  string gerber_path = gerber_directory;
  gerber_path += "/";
  gerber_path += gerber_file;
  auto g = GerberImporter();
  BOOST_REQUIRE(g.load_file(gerber_path));
  multi_polygon_type_fp polys = g.render(fill_closed_lines, 30);
  box_type_fp bounding_box;
  bg::envelope(polys, bounding_box);
  double min_x = bounding_box.min_corner().x();
  double min_y = bounding_box.min_corner().y();
  double max_x = bounding_box.max_corner().x();
  double max_y = bounding_box.max_corner().y();
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface = create_cairo_surface((max_x - min_x) * dpi, (max_y - min_y) * dpi);
  boost_bitmap_from_gerber(polys, min_x, min_y, max_x-min_x, max_y-min_y, cairo_surface);
  auto counts = get_counts(cairo_surface);
  size_t total = 0, marked = 0l;
  for (const auto& kv : counts) {
    switch (kv.first) {
      case NEW_BACKGROUND_COLOR:
        total += kv.second;
        break;
      default:
        marked += kv.second;
        total += kv.second;
        break;
    }
  }
  double marked_ratio = double(marked) / total;
  auto old_precision = std::cout.precision(3);
  auto old_width = std::cout.width(40);
  std::cout << gerber_file;
  std::cout.width(old_width);
  std::cout << "\t marked rate: " << marked_ratio*100 << "%"
            << "\t expected marked rate: [" << min_set_ratio*100 << ":" << max_set_ratio*100 << "]"
            << std::endl;
  std::cout.precision(old_precision);
  BOOST_CHECK_GE(marked_ratio, min_set_ratio);
  BOOST_CHECK_LE(marked_ratio, max_set_ratio);
  write_to_png(cairo_surface, gerber_file);
}

BOOST_AUTO_TEST_CASE(all_gerbers) {
  const char *skip_test = std::getenv("SKIP_GERBERIMPORTER_TESTS");
  if (skip_test != nullptr) {
    std::cout << "Skipping many gerberimporter tests because SKIP_GERBERIMPORTER_TESTS is set in environment." << std::endl;
    return;
  }

  test_one("levels.gbr",                  0.0007);
  test_one("levels_step_and_repeat.gbr",  0.006);
  test_one("code22_lower_left_line.gbr",  0.008);
  test_one("code4_outline.gbr",           0.023);
  test_one("code5_polygon.gbr",           0.00008);
  test_one("code21_center_line.gbr",      0.013);
  test_one("polygon.gbr",                 0.017);
  test_one("wide_oval.gbr",               0.00011);
  test_one("tall_oval.gbr",               0.00006);
  test_one("circle_oval.gbr",             0.00016);
  test_one("rectangle.gbr",               0.00007);
  test_one("circle.gbr",                  0.00008);
  test_one("code1_circle.gbr",            0.009);
  test_one("code20_vector_line.gbr",      0.013);
  test_one("g01_rectangle.gbr",           0.0008);
  test_one("moire.gbr",                   0.020);
  test_one("thermal.gbr",                 0.011);
  test_one("cutins.gbr",                  0.000);

  test_visual("circular_arcs.gbr", false, 0.077,    0.078);
  test_visual("broken_box.gbr",    true,  0.700,    0.701);
}

BOOST_AUTO_TEST_CASE(gerbv_exceptions) {
  auto g = GerberImporter();
  BOOST_CHECK(!g.load_file("foo.gbr"));
}

BOOST_AUTO_TEST_SUITE_END()
