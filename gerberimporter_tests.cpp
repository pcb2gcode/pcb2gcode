#define BOOST_TEST_MODULE gerberimporter tests
#include <boost/test/included/unit_test.hpp>

#include "gerberimporter.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <glibmm/init.h>
#include <gdkmm/wrap_init.h>
//#include <gdkmm/pixbuf.h>
#include <sstream>
#include <librsvg-2.0/librsvg/rsvg.h>
//#include <ImageMagick-6/Magick++.h>
#include <boost/format.hpp>
#include <cstdlib>

struct Fixture {
  Fixture() {
    Glib::init();
    Gdk::wrap_init();
    //MagickCore::InitializeMagick(boost::unit_test::framework::master_test_suite().argv[0]);
  }
  ~Fixture() {}
};

BOOST_FIXTURE_TEST_SUITE(gerberimporter_tests, Fixture);

const unsigned int dpi = 1000;

// Make a surface of the right size for the input gerber.
Cairo::RefPtr<Cairo::ImageSurface> create_cairo_surface(double width, double height) {
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface =
      Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32,
                                  width,
                                  height);
  // Set it all black.
  guint8* pixels = cairo_surface->get_data();
  int stride = cairo_surface->get_stride();
  for (int y = 0; y < cairo_surface->get_height(); y++) {
    for (int x = 0; x < cairo_surface->get_width(); x++) {
      *(reinterpret_cast<uint32_t *>(pixels + x*4 + y*stride)) = 0x00000000; // CLEAR
    }
  }
  return cairo_surface;
}

// Given a gerber file, return a pixmap that is a rasterized version of that
// gerber.  Uses gerbv's built-in utils.
void bitmap_from_gerber(const GerberImporter& g, double min_x, double min_y, double width, double height,
                                                      Cairo::RefPtr<Cairo::ImageSurface> cairo_surface) {
  //Render
  GdkColor blue = {0, 0, 0, 0xFFFF};
  g.render(cairo_surface, dpi, min_x, min_y, blue);
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
            % ((min_x - svg_bounding_box.min_corner().x() / 1000000) * dpi)
            // max and not min because the origin is in a different corner for svg vs gbr
            % ((svg_bounding_box.max_corner().y() / 1000000 - max_y) * dpi)
            % (svg_width * dpi / 1000000)
            % (svg_height * dpi / 1000000));
    bg::svg_mapper<point_type_fp> svg(svg_stream,
                                      svg_width * dpi / 1000000,
                                      svg_height * dpi / 1000000,
                                      svg_dimensions);
    svg.add(polys); // This is needed for the next line to work, not sure why.
    svg.map(polys, "fill-opacity:0.5;fill:rgb(255,0,0);");
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
  if(!rsvg_handle_render_cairo(rsvg_handle, cr->cobj())) {
    printf("to cairo failed\n");
    exit(2);
  }
}

const string gerber_directory = "testing/gerberimporter";

map<uint32_t, size_t> get_counts(Cairo::RefPtr<Cairo::ImageSurface> cairo_surface) {
  size_t background = 0, blue = 0, red = 0, both = 0, unknown = 0;
  // We only care about a few colors: background, match, red, blue, and all the rest.
  guint8* pixels = cairo_surface->get_data();
  int stride = cairo_surface->get_stride();
  for (int y = 0; y < cairo_surface->get_height(); y++) {
    for (int x = 0; x < cairo_surface->get_width(); x++) {
      auto *current_color = reinterpret_cast<uint32_t *>(pixels + x*4 + y*stride);
      switch (*current_color) {
        case 0:
          background++;
          break;
        case 0xff80007f:
          both++;
          // Make it stick out less so that we can better see errors.
          *current_color = 0x2000ff00;
          break;
        case 0xff0000ff:
          blue++;
          break;
        case 0x80800000:
          red++;
          break;
        default:
          unknown++;
          break;
      }
    }
  }
  map<uint32_t, size_t> counts;
  counts[0] = background;
  counts[0x2000ff00] = both;
  counts[0xff0000ff] = blue;
  counts[0x80800000] = red;
  counts[0x12345678] = unknown;
  return counts;
}

// Compare gerbv image against boost generated image.
void test_one(const string& gerber_file, double max_error_rate) {
  string gerber_path = gerber_directory;
  gerber_path += "/";
  gerber_path += gerber_file;
  auto g = GerberImporter(gerber_path);
  multi_polygon_type_fp polys = g.render(false, 30);
  box_type_fp bounding_box;
  bg::envelope(polys, bounding_box);
  double min_x = std::min(g.get_min_x(), bounding_box.min_corner().x() / g.vectorial_scale());
  double min_y = std::min(g.get_min_y(), bounding_box.min_corner().y() / g.vectorial_scale());
  double max_x = std::max(g.get_max_x(), bounding_box.max_corner().x() / g.vectorial_scale());
  double max_y = std::max(g.get_max_y(), bounding_box.max_corner().y() / g.vectorial_scale());
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface = create_cairo_surface((max_x - min_x) * dpi, (max_y - min_y) * dpi);
  bitmap_from_gerber(g, min_x, min_y, max_x-min_x, max_y-min_y, cairo_surface);
  boost_bitmap_from_gerber(polys, min_x, min_y, max_x-min_x, max_y-min_y, cairo_surface);
  map<uint32_t, size_t> counts = get_counts(cairo_surface);
  size_t background = 0, errors = 0, both = 0;
  for (const auto& kv : counts) {
    //printf("%x %ld\n", kv.first, kv.second);
    switch (kv.first) {
      case 0:
        background += kv.second;
        break;
      case 0x2000ff00:
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
  const char *skip_png = std::getenv("SKIP_GERBERIMPORTER_TESTS_PNG");
  if (skip_png != nullptr) {
    std::cout << "Skipping png generation because SKIP_GERBERIMPORTER_TESTS_PNG is set in environment." << std::endl;
    return;
  }
  cairo_surface->write_to_png(str(boost::format("%s.png") % gerber_file).c_str());
}

// For cases when even gerbv is wrong, just check that the number of pixels
// marked is more or less correct.  Look at http://www.gerber-viewer.com/ to
// test these.
void test_visual(const string& gerber_file, double min_set_ratio, double max_set_ratio) {
  string gerber_path = gerber_directory;
  gerber_path += "/";
  gerber_path += gerber_file;
  auto g = GerberImporter(gerber_path);
  multi_polygon_type_fp polys = g.render(false, 30);
  box_type_fp bounding_box;
  bg::envelope(polys, bounding_box);
  double min_x = bounding_box.min_corner().x() / g.vectorial_scale();
  double min_y = bounding_box.min_corner().y() / g.vectorial_scale();
  double max_x = bounding_box.max_corner().x() / g.vectorial_scale();
  double max_y = bounding_box.max_corner().y() / g.vectorial_scale();
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface = create_cairo_surface((max_x - min_x) * dpi, (max_y - min_y) * dpi);
  boost_bitmap_from_gerber(polys, min_x, min_y, max_x-min_x, max_y-min_y, cairo_surface);
  auto counts = get_counts(cairo_surface);
  size_t total = 0, marked = 0l;
  for (const auto& kv : counts) {
    switch (kv.first) {
      case 0:
        total += kv.second;
        break;
      default:
        marked += kv.second;
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
  const char *skip_png = std::getenv("SKIP_GERBERIMPORTER_TESTS_PNG");
  if (skip_png != nullptr) {
    std::cout << "Skipping png generation because SKIP_GERBERIMPORTER_TESTS_PNG is set in environment." << std::endl;
    return;
  }
  cairo_surface->write_to_png(str(boost::format("%s.png") % gerber_file).c_str());
}

BOOST_AUTO_TEST_CASE(all_gerbers) {
  const char *skip_test = std::getenv("SKIP_GERBERIMPORTER_TESTS");
  if (skip_test != nullptr) {
    std::cout << "Skipping many gerberimporter tests because SKIP_GERBERIMPORTER_TESTS is set in environment." << std::endl;
    return;
  }

  test_one("levels.gbr",                  0.005);
  test_one("levels_step_and_repeat.gbr",  0.013);
  test_one("code22_lower_left_line.gbr",  0.011);
  test_one("code4_outline.gbr",           0.025);
  test_one("code5_polygon.gbr",           0.0007);
  test_one("code21_center_line.gbr",      0.020);
  test_one("polygon.gbr",                 0.022);
  test_one("wide_oval.gbr",               0.015);
  test_one("tall_oval.gbr",               0.005);
  test_one("circle_oval.gbr",             0.017);
  test_one("rectangle.gbr",               0.009);
  test_one("circle.gbr",                  0.009);
  test_one("code1_circle.gbr",            0.015);
  test_one("code20_vector_line.gbr",      0.018);
  test_one("g01_rectangle.gbr",           0.002);
  test_one("moire.gbr",                   0.05);
  test_one("thermal.gbr",                 0.019);
  test_one("cutins.gbr",                  0.000);

  test_visual("circular_arcs.gbr",        0.083,    0.085);
}

BOOST_AUTO_TEST_CASE(gerbv_exceptions) {
  BOOST_CHECK_THROW(GerberImporter("foo.gbr"), gerber_exception);
}

BOOST_AUTO_TEST_SUITE_END()
