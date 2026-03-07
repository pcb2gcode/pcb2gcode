#define BOOST_TEST_MODULE gerberimporter tests
#include <boost/test/unit_test.hpp>
#include <boost/test/framework.hpp>

#include "gerberimporter.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <glibmm/init.h>
#include <gdkmm/wrap_init.h>
#include <sstream>
#include <librsvg-2.0/librsvg/rsvg.h>
#include <boost/format.hpp>
#include <cstdlib>
#include <cairomm/cairomm.h>
#include <string>
using std::string;

#include <map>
using std::map;
#include <vector>
using std::vector;

#include <yaml-cpp/yaml.h>

#include "config.h"

struct GerberimporterTestConfig {
  vector<std::tuple<string, double>> match_gerbv;
  vector<std::tuple<string, bool, double>> visual;
};

static string normalize_boost_version(string v) {
  for (auto& c : v) if (c == '_') c = '.';
  return v;
}

static GerberimporterTestConfig load_gerberimporter_test_config(const string& path,
                                                               const string& geos_version,
                                                               const string& boost_version) {
  GerberimporterTestConfig cfg;
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const YAML::BadFile&) {
    BOOST_REQUIRE_MESSAGE(false, "Failed to open test config: " << path);
  }
  const string boost_norm = normalize_boost_version(boost_version);
  const YAML::Node& configs = root["configs"];
  BOOST_REQUIRE_MESSAGE(configs && configs.IsSequence(), "YAML must have a 'configs' sequence");
  const YAML::Node* selected = nullptr;
  const YAML::Node* default_config = nullptr;
  for (const auto& config : configs) {
    string g = config["geos_version"] ? config["geos_version"].as<string>() : "";
    string b = config["boost_version"] ? normalize_boost_version(config["boost_version"].as<string>()) : "";
    if (g == geos_version && b == boost_norm) {
      selected = &config;
      break;
    }
    if (g == "default" && b == "default")
      default_config = &config;
  }
  if (selected == nullptr)
    selected = default_config;
  BOOST_REQUIRE_MESSAGE(selected != nullptr,
    "No config for geos_version=" << geos_version << " boost_version=" << boost_version
    << "; add a matching config or use default/default in " << path);
  const YAML::Node& match = (*selected)["match_gerbv"];
  if (match) {
    for (const auto& entry : match) {
      cfg.match_gerbv.push_back(std::make_tuple(
        entry["gerber_file"].as<string>(),
        entry["expected_error_rate"].as<double>()));
    }
  }
  const YAML::Node& visual = (*selected)["visual"];
  if (visual) {
    for (const auto& entry : visual) {
      cfg.visual.push_back(std::make_tuple(
        entry["gerber_file"].as<string>(),
        entry["fill_closed_lines"].as<bool>(),
        entry["expected_set_ratio"].as<double>()));
    }
  }
  return cfg;
}

static string get_test_arg(const string& long_opt, const string& short_env_key) {
  const auto& mts = boost::unit_test::framework::master_test_suite();
  const string with_equals = long_opt + "=";
  for (int i = 1; i < mts.argc; i++) {
    string arg(mts.argv[i]);
    if (arg.compare(0, with_equals.size(), with_equals) == 0)
      return arg.substr(with_equals.size());
    if (arg == long_opt && i + 1 < mts.argc)
      return string(mts.argv[i + 1]);
  }
  const char* env = std::getenv(short_env_key.c_str());
  if (env && env[0] != '\0')
    return string(env);
  return "default";
}

static string gerberimporter_tests_yaml_path() {
  const auto& mts = boost::unit_test::framework::master_test_suite();
  const string prefix = "--gerberimporter-tests-yaml=";
  for (int i = 1; i < mts.argc; i++) {
    string arg(mts.argv[i]);
    if (arg.compare(0, prefix.size(), prefix) == 0)
      return arg.substr(prefix.size());
    if (arg == "--gerberimporter-tests-yaml" && i + 1 < mts.argc)
      return string(mts.argv[i + 1]);
  }
  return "tests/data/gerberimporter_tests.yaml";
}

struct Fixture {
  Fixture() {
    Glib::init();
    Gdk::wrap_init();
  }
  ~Fixture() {}
};

BOOST_FIXTURE_TEST_SUITE(gerberimporter_tests, Fixture)

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

void cairo_render(const GerberImporter& g, Cairo::RefPtr<Cairo::ImageSurface> surface, const guint dpi,
                  const double min_x, const double min_y,
                  const GdkColor& color, const gerbv_render_types_t& renderType) {
  gerbv_render_info_t render_info;

  render_info.scaleFactorX = dpi;
  render_info.scaleFactorY = dpi;
  render_info.lowerLeftX = min_x;
  render_info.lowerLeftY = min_y;
  render_info.displayWidth = surface->get_width();
  render_info.displayHeight = surface->get_height();
  render_info.renderType = renderType;
  render_info.show_cross_on_drill_holes = false;

  g.get_project()->file[0]->color = color;

  Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surface);
  gerbv_render_layer_to_cairo_target(cr->cobj(), g.get_project()->file[0], &render_info);

  /// @todo check wheter importing was successful
}

// Given a gerber file, return a pixmap that is a rasterized version of that
// gerber.  Uses gerbv's built-in utils.
void bitmap_from_gerber(const GerberImporter& g, const point_type_fp& min_corner,
                        Cairo::RefPtr<Cairo::ImageSurface> cairo_surface) {
  //Render
  GdkColor blue = {0,
                   ((OLD_COLOR >> 16) & 0xff) * 0x101,
                   ((OLD_COLOR >>  8) & 0xff) * 0x101,
                   ((OLD_COLOR      ) & 0xff) * 0x101};
  cairo_render(g, cairo_surface, dpi, min_corner.x(), min_corner.y(), blue, GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY);
}

string render_svg(const multi_polygon_type_fp& polys, const box_type_fp& bounding_box) {
  std::stringstream svg_stream;
  {
    box_type_fp svg_bounding_box;
    bg::envelope(polys, svg_bounding_box);
    const double svg_width = (svg_bounding_box.max_corner().x() - svg_bounding_box.min_corner().x());
    const double svg_height = (svg_bounding_box.max_corner().y() - svg_bounding_box.min_corner().y());
    const string svg_dimensions =
        str(boost::format("width=\"%1%px\" height=\"%2%px\" viewBox=\"%3% %4% %5% %6%\"")
            % (svg_width * dpi)
            % (svg_height * dpi)
            % ((bounding_box.min_corner().x() - svg_bounding_box.min_corner().x()) * dpi)
            // max and not min because the origin is in a different corner for svg vs gbr
            % ((svg_bounding_box.max_corner().y() - bounding_box.max_corner().y()) * dpi)
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
void boost_bitmap_from_gerber(const multi_polygon_type_fp& polys, const box_type_fp& bounding_box,
                              Cairo::RefPtr<Cairo::ImageSurface> cairo_surface) {
  string svg_string = render_svg(polys, bounding_box);
  //Now we have the svg, let's make a cairo surface like above.
  GError* gerror = nullptr;
  RsvgHandle *rsvg_handle = rsvg_handle_new_from_data(reinterpret_cast<const guint8*>(svg_string.c_str()),
                                                      svg_string.size(),
                                                      &gerror);
  Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(cairo_surface);
#ifdef HAVE_RSVG_HANDLE_RENDER_DOCUMENT
  RsvgRectangle viewport;
  viewport.x = 0;
  viewport.y = 0;
#ifdef HAVE_RSVG_HANDLE_GET_INTRINSIC_SIZE_IN_PIXELS
  rsvg_handle_set_dpi(rsvg_handle, 96);
  BOOST_REQUIRE(rsvg_handle_get_intrinsic_size_in_pixels(
                    rsvg_handle, &viewport.width, &viewport.height));
#else // !HAVE_RSVG_HANDLE_GET_INTRINSIC_SIZE_IN_PIXELS
  RsvgDimensionData dimensions;
  rsvg_handle_get_dimensions(rsvg_handle, &dimensions);
  viewport.width = dimensions.width;
  viewport.height = dimensions.height;
#endif
  BOOST_REQUIRE(rsvg_handle_render_document(rsvg_handle, cr->cobj(), &viewport, nullptr));
#else // !HAVE_RSVG_HANDLE_RENDER_DOCUMENT
  BOOST_REQUIRE(rsvg_handle_render_cairo(rsvg_handle, cr->cobj()));
#endif // HAVE_RSVG_HANDLE_RENDER_DOCUMENT
  g_object_unref(rsvg_handle);
}

const string default_gerber_directory = "tests/data/gerberimporter";

const string& gerber_directory() {
  static const string configured_dir = []() {
    const auto& mts = boost::unit_test::framework::master_test_suite();
    for (int i = 1; i < mts.argc; i++) {
      const string arg(mts.argv[i]);
      const string long_form = "--gerber-root=";
      if (arg.compare(0, long_form.size(), long_form) == 0) {
        return arg.substr(long_form.size());
      }
      if (arg == "--gerber-root" && i + 1 < mts.argc) {
        return string(mts.argv[i + 1]);
      }
      if (!arg.empty() && arg[0] != '-') {
        // Any positional custom argument is treated as the gerber fixtures root.
        return arg;
      }
    }
    return default_gerber_directory;
  }();
  return configured_dir;
}

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
    return;
  }
  cairo_surface->write_to_png(str(boost::format("%s.png") % gerber_file).c_str());
}

coordinate_type_fp width(box_type_fp box) {
  return box.max_corner().x() - box.min_corner().x();
}

coordinate_type_fp height(box_type_fp box) {
  return box.max_corner().y() - box.min_corner().y();
}

// Compare gerbv image against boost generated image.
void test_one(const string& gerber_file, double expected_error_rate) {
  string gerber_path = gerber_directory();
  gerber_path += "/";
  gerber_path += gerber_file;
  auto g = GerberImporter(0.0004);
  BOOST_REQUIRE(g.load_file(gerber_path));
  multi_polygon_type_fp polys = g.render(false, true).first;
  box_type_fp bounding_box;
  bg::envelope(polys, bounding_box);
  bg::expand(bounding_box, g.get_bounding_box());
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface = create_cairo_surface(width(bounding_box) * dpi, height(bounding_box) * dpi);
  bitmap_from_gerber(g, bounding_box.min_corner(), cairo_surface);
  boost_bitmap_from_gerber(polys, bounding_box, cairo_surface);
  map<uint32_t, size_t> counts = get_counts(cairo_surface);
  size_t errors = 0, both = 0;
  for (const auto& kv : counts) {
    switch (kv.first) {
      case NEW_BACKGROUND_COLOR:
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
  auto old_precision = std::cout.precision(4);
  auto old_width = std::cout.width(40);
  std::cout << gerber_file;
  std::cout.width(old_width);
  std::cout << "\t error rate: " << error_rate*100 << "%"
            << "\t expected: " << expected_error_rate*100 << "%"
            << std::endl;
  std::cout.precision(old_precision);
  BOOST_CHECK_CLOSE(error_rate, expected_error_rate, 0.1);
  write_to_png(cairo_surface, gerber_file);
}

// For cases when even gerbv is wrong, just check that the number of pixels
// marked is more or less correct.  Look at http://www.gerber-viewer.com/ to
// test these.
void test_visual(const string& gerber_file, bool fill_closed_lines, double expected_set_ratio) {
  string gerber_path = gerber_directory();
  gerber_path += "/";
  gerber_path += gerber_file;
  auto g = GerberImporter(0.0004);
  BOOST_REQUIRE(g.load_file(gerber_path));
  multi_polygon_type_fp polys = g.render(fill_closed_lines, true).first;
  box_type_fp bounding_box;
  bg::envelope(polys, bounding_box);
  Cairo::RefPtr<Cairo::ImageSurface> cairo_surface = create_cairo_surface(width(bounding_box) * dpi, height(bounding_box) * dpi);
  boost_bitmap_from_gerber(polys, bounding_box, cairo_surface);
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
  auto old_precision = std::cout.precision(4);
  auto old_width = std::cout.width(40);
  std::cout << gerber_file;
  std::cout.width(old_width);
  std::cout << "\t marked rate: " << marked_ratio*100 << "%"
            << "\t expected marked rate: " << expected_set_ratio*100 << "%"
            << std::endl;
  std::cout.precision(old_precision);
  BOOST_CHECK_CLOSE(marked_ratio, expected_set_ratio, 0.1);
  write_to_png(cairo_surface, gerber_file);
}

BOOST_AUTO_TEST_CASE(gerberimporter_match_gerbv) {
  const char* skip_test = std::getenv("SKIP_GERBERIMPORTER_TESTS");
  if (skip_test != nullptr) {
    std::cout << "Skipping because SKIP_GERBERIMPORTER_TESTS is set in environment." << std::endl;
    return;
  }
  string geos_ver = get_test_arg("--geos-version", "GEOS_VERSION");
  string boost_ver = get_test_arg("--boost-version", "BOOST_VERSION");
  GerberimporterTestConfig cfg = load_gerberimporter_test_config(
    gerberimporter_tests_yaml_path(), geos_ver, boost_ver);
  for (const auto& entry : cfg.match_gerbv) {
    test_one(std::get<0>(entry), std::get<1>(entry));
  }
}

BOOST_AUTO_TEST_CASE(gerberimporter_visual) {
  const char* skip_test = std::getenv("SKIP_GERBERIMPORTER_TESTS");
  if (skip_test != nullptr) {
    std::cout << "Skipping because SKIP_GERBERIMPORTER_TESTS is set in environment." << std::endl;
    return;
  }
  string geos_ver = get_test_arg("--geos-version", "GEOS_VERSION");
  string boost_ver = get_test_arg("--boost-version", "BOOST_VERSION");
  GerberimporterTestConfig cfg = load_gerberimporter_test_config(
    gerberimporter_tests_yaml_path(), geos_ver, boost_ver);
  for (const auto& entry : cfg.visual) {
    test_visual(std::get<0>(entry), std::get<1>(entry), std::get<2>(entry));
  }
}

BOOST_AUTO_TEST_CASE(gerbv_exceptions) {
  auto g = GerberImporter(0.0004);
  BOOST_CHECK(!g.load_file("foo.gbr"));
}

BOOST_AUTO_TEST_SUITE_END()
