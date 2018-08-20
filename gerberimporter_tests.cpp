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

struct Fixture {
  Fixture() {
    BOOST_TEST_MESSAGE( "setup fixture" );
    Glib::init();
    Gdk::wrap_init();
    //MagickCore::InitializeMagick(boost::unit_test::framework::master_test_suite().argv[0]);
  }
  ~Fixture() {
    BOOST_TEST_MESSAGE( "teardown fixture" );
  }
};

BOOST_FIXTURE_TEST_SUITE(gerberimporter_tests, Fixture);

const unsigned int dpi = 10;
const unsigned int procmargin = 0;

// Given a gerber file, return a pixmap that is a rasterized version of that
// gerber.  Uses gerbv's built-in utils.
vector<vector<bool>> bitmap_from_gerber(const GerberImporter& g) {
  Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, g.get_width()*dpi+2*procmargin, g.get_height()*dpi+2*procmargin);
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface =
      Cairo::ImageSurface::create(pixbuf->get_pixels(),
                                  Cairo::FORMAT_ARGB32,
                                  g.get_width()  * dpi + 2 * procmargin,
                                  g.get_height() * dpi + 2 * procmargin,
                                  pixbuf->get_rowstride());
  guint8* pixels = cairo_surface->get_data();
  int stride = cairo_surface->get_stride();
  for (int y = 0; y < pixbuf->get_height(); y++) {
    for (int x = 0; x < pixbuf->get_width(); x++) {
      *(reinterpret_cast<uint32_t *>(pixels + x*4 + y*stride)) = 0xFF000000; // BlACK
    }
  }
  g.render(cairo_surface, dpi,
           g.get_min_x() - static_cast<double>(procmargin) / dpi,
           g.get_min_y() - static_cast<double>(procmargin) / dpi);
  vector<vector<bool>> grid;
  grid.resize(pixbuf->get_height());
  for (int y = 0; y < pixbuf->get_height(); y++) {
    grid[y].resize(pixbuf->get_width());
    for (int x = 0; x < pixbuf->get_width(); x++) {
      if (*(reinterpret_cast<const uint32_t *>(pixels + x*4 + y*stride)) == 0xFF000000) {
        grid[y][x] = true;
      } else {
        grid[y][x] = false;
      }
    }
  }
  return grid;
}

// Convert the gerber to a boost geometry and then conver that to SVG and then rasterize to a bitmap.
vector<vector<bool>> boost_bitmap_from_gerber(const GerberImporter& g) {
  multi_polygon_type_fp polys = g.render(false, 30);
  std::cout << bg::wkt(polys) << std::endl;
  std::stringstream svg_stream;
  box_type_fp svg_bounding_box;
  bg::envelope(polys, svg_bounding_box);
  double width = (svg_bounding_box.max_corner().x() - svg_bounding_box.min_corner().x()) * dpi / 1000000;
  double height = (svg_bounding_box.max_corner().y() - svg_bounding_box.min_corner().y()) * dpi / 1000000;
  {
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"0 0 %1% %2%\"") % width % height);
    bg::svg_mapper<point_type_fp> svg(svg_stream,
                                      width,
                                      height,
                                      svg_dimensions);
    svg.add(svg_bounding_box); // This is needed for the next line to work, not sure why.
    svg.map(polys, "fill-opacity:1.0;fill:rgb(0,255,255);");
  }

  //Now we have the svg, let's make a cairo surface like above.
  GError* gerror = nullptr;
  RsvgHandle *rsvg_handle = rsvg_handle_new_from_data(reinterpret_cast<const guint8*>(svg_stream.str().c_str()),
                                                      svg_stream.str().size(),
                                                      &gerror);
  //rsvg_handle_set_dpi(rsvg_handle, dpi*1000);
  width = 100;
  height = 100;

  Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, g.get_width()*dpi+2*procmargin, g.get_height()*dpi+2*procmargin);
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface =
      Cairo::ImageSurface::create(pixbuf->get_pixels(),
                                  Cairo::FORMAT_ARGB32,
                                  g.get_width()  * dpi + 2 * procmargin,
                                  g.get_height() * dpi + 2 * procmargin,
                                  pixbuf->get_rowstride());
  cairo_t *cr = cairo_create(cairo_surface->cobj());
  guint8* pixels = cairo_surface->get_data();
  int stride = cairo_surface->get_stride();
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      *(reinterpret_cast<uint32_t *>(pixels + x*4 + y*stride)) = 0xFF000000; // BLACK
    }
  }
  if(!rsvg_handle_render_cairo(rsvg_handle, cr)) {
    printf("to cairo failed\n");
    exit(2);
  }
  vector<vector<bool>> grid;
  grid.resize(height);
  for (int y = 0; y < height; y++) {
    grid[y].resize(width);
    for (int x = 0; x < width; x++) {
      if (*(reinterpret_cast<const uint32_t *>(pixels + x*4 + y*stride)) == 0xFF000000) {
        grid[y][x] = true;
      } else {
        grid[y][x] = false;
      }
    }
  }
  cairo_destroy(cr);
  return grid;
}

BOOST_AUTO_TEST_CASE(all_gerbers) {
  const string gerber_directory = "testing/gerberimporter";
  DIR *dirp = opendir(gerber_directory.c_str());
  dirent *entry;
  while ((entry = readdir(dirp)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 ||
        strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    string gerber_file = gerber_directory;
    gerber_file += "/";
    gerber_file += entry->d_name;
    auto g = GerberImporter(gerber_file);
    vector<vector<bool>> grid = bitmap_from_gerber(g);
    for (const auto& y : grid) {
      for (const auto& x : y) {
        printf("%d", x);
      }
      printf("\n");
    }
    vector<vector<bool>> grid2 = boost_bitmap_from_gerber(g);
    for (const auto& y : grid2) {
      for (const auto& x : y) {
        printf("%d", x);
      }
      printf("\n");
    }
  }
  closedir(dirp);
}

BOOST_AUTO_TEST_SUITE_END()



  /*
  auto pixbuf = rsvg_handle_get_pixbuf(rsvg_handle);
  auto stride = gdk_pixbuf_get_rowstride(pixbuf);
  vector<vector<bool>> grid;
  auto height = gdk_pixbuf_get_height(pixbuf);
  auto width = gdk_pixbuf_get_width(pixbuf);
  auto pixels = gdk_pixbuf_read_pixels(pixbuf);
  grid.resize(height);
  for (int y = 0; y < height; y++) {
    grid[y].resize(width);
    for (int x = 0; x < width; x++) {
      printf("%d\n", *(reinterpret_cast<const uint32_t *>(pixels + x*4 + y*stride)));
      if (*(reinterpret_cast<uint32_t *>(pixels + x*4 + y*stride)) == 0xFF000000) {
        grid[y][x] = true;
      } else {
        grid[y][x] = false;
        }
    }
  }
  rsvg_handle_close(rsvg_handle, &gerror);
  g_object_unref(rsvg_handle);
  return grid;
*/



  /*  Magick::Image image;
  printf("%s\n", svg_stream.str().c_str());
  std::ofstream out("test.svg");
  out << svg_stream.str();
  out.close();
  //Magick::Blob blob(svg_stream.str().c_str(), svg_stream.str().size());
  //image.read(blob,
  //           Magick::Geometry((boost::format("%dx%d+%d+%d") % width % height % procmargin % procmargin).str()),
  //           8,
  //           "SVG");
  image.read("test.svg");
  image.write("test.png");
  char pixels[size_t(width*height*4)];
  for (unsigned int i = 0; i < width*height*4; i++) {
    pixels[i] = 0;
  }
  image.write(0,0,width,height,"ARGB",Magick::CharPixel,pixels);
  for (unsigned int i = 0; i < width*height*4; i++) {
    printf("%d\n", pixels[i]);
  }
  vector<vector<bool>> grid;
  //grid.resize(height);
  return grid;*/
