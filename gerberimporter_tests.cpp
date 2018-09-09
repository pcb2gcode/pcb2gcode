#define BOOST_TEST_MODULE gerberimporter tests
#include <boost/test/included/unit_test.hpp>

#include "gerberimporter.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <glibmm/init.h>
#include <gdkmm/wrap_init.h>
#include <gdkmm/pixbuf.h>
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

class Grid {
 public:
  Grid() {}
  Grid(const Cairo::RefPtr<Cairo::ImageSurface> cairo_surface, char background, char foreground) {
    guint8* pixels = cairo_surface->get_data();
    int stride = cairo_surface->get_stride();
    grid.resize(cairo_surface->get_height());
    for (int y = 0; y < cairo_surface->get_height(); y++) {
      grid[y].resize(cairo_surface->get_width());
      for (int x = 0; x < cairo_surface->get_width(); x++) {
        auto current_color = *(reinterpret_cast<const uint32_t *>(pixels + x*4 + y*stride));
        // 0x0 is polarity clear
        // 0xff000000 is untouched, should be also clear
        // 0xffffffff is polarity dark
        if (current_color == 0xFFFFFFFF) {
          grid[y][x] = foreground;
        } else {
          grid[y][x] = background;
        }
      }
    }
  }
  const vector<vector<char>> get_grid() const {
    return grid;
  }
  void write_xpm3(std::ostream& os, const vector<string>& colors) const {
    os << "/* XPM */" << std::endl;
    os << "static char * grid_xpm[] = {" << std::endl;
    os << '"' << ((grid.size() > 0) ? grid[0].size()  : 0) << " "; // width
    os << grid.size() << " "; //height
    os << colors.size() << " "; // color count
    os << "1\"," << std::endl; // One character per pixel
    for (const auto& color : colors) {
      os << '"' << color << "\"," << std::endl;
    }
    for (const auto& row : grid) {
      os << '"';
      for (const auto& c : row) {
        os << c;
      }
      os << "\"," << std::endl;
    }
    os << '}' << std::endl;
  }
  Grid& operator|=(const Grid& rhs) {
    for (unsigned int i = 0; i < grid.size() && i < rhs.grid.size(); i++) {
      for (unsigned int j = 0; j < grid[i].size() && j < rhs.grid[i].size(); j++) {
        this->grid[i][j] |= rhs.grid[i][j];
      }
    }
    return *this;
  }
  map<char, unsigned int> get_counts() const {
    unsigned int all_values[256];
    for (unsigned int i = 0; i < 256; i++) {
      all_values[i] = 0;
    }
    for (unsigned int i = 0; i < grid.size(); i++) {
      for (unsigned int j = 0; j < grid[i].size(); j++) {
        ++all_values[size_t(this->grid[i][j])];
      }
    }
    map<char, unsigned int> counts;
    for (unsigned int i = 0; i < 256; i++) {
      if (all_values[i] != 0) {
        counts[char(i)] = all_values[i];
      }
    }
    return counts;
  }

 private:
  vector<vector<char>> grid;
};

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
      *(reinterpret_cast<uint32_t *>(pixels + x*4 + y*stride)) = 0xFF000000; // BLACK
    }
  }
  return cairo_surface;
}

// Given a gerber file, return a pixmap that is a rasterized version of that
// gerber.  Uses gerbv's built-in utils.
Grid bitmap_from_gerber(const GerberImporter& g, double min_x, double min_y, double width, double height) {
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface = create_cairo_surface(width * dpi, height * dpi);

  //Render
  g.render(cairo_surface, dpi, min_x, min_y);

  // Make the grid.
  return Grid(cairo_surface, '.', 'O');
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
    svg.map(polys, "fill-opacity:1.0;fill:rgb(255,255,255);");
  } // The svg file is complete when it goes out of scope.
  return svg_stream.str();
}

// Convert the gerber to a boost geometry and then convert that to SVG and then rasterize to a bitmap.
Grid boost_bitmap_from_gerber(const multi_polygon_type_fp& polys, double min_x, double min_y, double width, double height) {
  string svg_string = render_svg(polys, min_x, min_y, width, height);
  //Now we have the svg, let's make a cairo surface like above.
  GError* gerror = nullptr;
  RsvgHandle *rsvg_handle = rsvg_handle_new_from_data(reinterpret_cast<const guint8*>(svg_string.c_str()),
                                                      svg_string.size(),
                                                      &gerror);
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface = create_cairo_surface(width * dpi, height * dpi);
  Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(cairo_surface);
  if(!rsvg_handle_render_cairo(rsvg_handle, cr->cobj())) {
    printf("to cairo failed\n");
    exit(2);
  }

  return Grid(cairo_surface, '.', 'X');
}

const string gerber_directory = "testing/gerberimporter";

// Compare gerbv image against boost generated image.
void test_one(const string& gerber_file, double max_error_rate) {
  string gerber_path = gerber_directory;
  gerber_path += "/";
  gerber_path += gerber_file;
  auto g = GerberImporter(gerber_path);
  multi_polygon_type_fp polys = g.render(false, 360);
  box_type_fp bounding_box;
  bg::envelope(polys, bounding_box);
  double min_x = std::min(g.get_min_x(), bounding_box.min_corner().x() / g.vectorial_scale());
  double min_y = std::min(g.get_min_y(), bounding_box.min_corner().y() / g.vectorial_scale());
  double max_x = std::max(g.get_max_x(), bounding_box.max_corner().x() / g.vectorial_scale());
  double max_y = std::max(g.get_max_y(), bounding_box.max_corner().y() / g.vectorial_scale());
  Grid grid = bitmap_from_gerber(g, min_x, min_y, max_x-min_x, max_y-min_y);
  Grid grid2 = boost_bitmap_from_gerber(polys, min_x, min_y, max_x-min_x, max_y-min_y);
  grid |= grid2;
  auto counts = grid.get_counts();
  unsigned int errors = counts['~'] + counts['o'];
  unsigned int total = errors + counts['_'];
  double error_rate = double(errors)/total;
  BOOST_CHECK_LE(error_rate, max_error_rate);
  auto old_precision = std::cout.precision(3);
  std::cout << gerber_file
            << "\t error rate: " << double(errors)/total*100 << "%"
            << "\t expected less than: " << max_error_rate*100 << "%"
            << std::endl;
  std::cout.precision(old_precision);
  std::ofstream xpm_out;
  xpm_out.open(str(boost::format("%s.xpm") % gerber_file).c_str());
  grid.write_xpm3(xpm_out,
                  { ". c #000000",
                    "~ c #FF0000",
                    "_ c #003300",
                    "o c #0000FF"
                  });
  xpm_out.close();
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
  Grid grid = boost_bitmap_from_gerber(polys, min_x, min_y, max_x-min_x, max_y-min_y);
  auto counts = grid.get_counts();
  unsigned int marked = counts['X'];
  unsigned int total = marked + counts['.'];
  double marked_ratio = double(marked) / total;
  BOOST_CHECK_GE(marked_ratio, min_set_ratio);
  BOOST_CHECK_LE(marked_ratio, max_set_ratio);
  auto old_precision = std::cout.precision(3);
  std::cout << gerber_file
            << "\t marked rate: " << marked_ratio*100 << "%"
            << "\t expected marked rate: [" << min_set_ratio*100 << ":" << max_set_ratio*100 << "]"
            << std::endl;
  std::cout.precision(old_precision);
  std::ofstream xpm_out;
  xpm_out.open(str(boost::format("%s.xpm") % gerber_file).c_str());
  grid.write_xpm3(xpm_out,
                  { ". c #000000",
                    "X c #0000FF"
                  });
  xpm_out.close();
}

BOOST_AUTO_TEST_CASE(all_gerbers) {
  const char *skip_test = std::getenv("SKIP_GERBERIMPORTER_TESTS");
  if (skip_test != nullptr) {
    std::cout << "Skipping many gerberimporter tests because SKIP_GERBERIMPORTER_TESTS is set in environment." << std::endl;
    return;
  }

  test_one("levels.gbr",                  0.0006);
  test_one("levels_step_and_repeat.gbr",  0.0067);
  test_one("code22_lower_left_line.gbr",  0.008);
  test_one("code4_outline.gbr",           0.023);
  test_one("code5_polygon.gbr",           0.00008);
  test_one("code21_center_line.gbr",      0.013);
  test_one("polygon.gbr",                 0.017);
  test_one("wide_oval.gbr",               0.00008);
  test_one("tall_oval.gbr",               0.00004);
  test_one("circle_oval.gbr",             0.00012);
  test_one("rectangle.gbr",               0.00007);
  test_one("circle.gbr",                  0.00004);
  test_one("code1_circle.gbr",            0.009);
  test_one("code20_vector_line.gbr",      0.011);
  test_one("g01_rectangle.gbr",           0.0008);
  test_one("moire.gbr",                   0.020);
  test_one("thermal.gbr",                 0.011);
  test_one("cutins.gbr",                  0.000);

  test_visual("circular_arcs.gbr",        0.075,    0.076);
}

BOOST_AUTO_TEST_CASE(gerbv_exceptions) {
  BOOST_CHECK_THROW(GerberImporter("foo.gbr"), gerber_exception);
}

BOOST_AUTO_TEST_SUITE_END()
