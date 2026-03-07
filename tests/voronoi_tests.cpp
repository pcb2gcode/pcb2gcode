#define BOOST_TEST_MODULE voronoi tests
#include <boost/test/unit_test.hpp>

#include <boost/format.hpp>
#include <fstream>
#include "voronoi.hpp"
#include "consistent_rand.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(voronoi_tests)

// Output SVG files for debugging.
#define DEBUG_SVG TRUE

template <typename geo_t>
void print_svg(const geo_t& geo, const string& filename) {
  std::ofstream svg(filename);

  box_type bounding_box{{0, 0}, {0, 0}};
  if (bg::area(geo) > 0) {
    bounding_box = bg::return_envelope<box_type>(geo);
  }
  bg::svg_mapper<point_type> mapper(svg,
                                    bounding_box.max_corner().x() - bounding_box.min_corner().x(),
                                    bounding_box.max_corner().y() - bounding_box.min_corner().y());
  ConsistentRand::srand(1);
  for (const auto& g : geo) {
    mapper.add(g);
    mapper.map(g, (boost::format("opacity:1;fill:rgb(%d,%d,%d);stroke:rgb(0,0,0);stroke-width:2")
                   % (ConsistentRand::rand()%255)
                   % (ConsistentRand::rand()%255)
                   % (ConsistentRand::rand()%255)).str());
  }
}

BOOST_AUTO_TEST_CASE(empty) {
  multi_polygon_type mp;
  box_type bounding_box;
  bg::envelope(mp, bounding_box);
  const auto& result = Voronoi::build_voronoi(mp, bounding_box, 10);
  BOOST_CHECK(result.size() == 0);
}

BOOST_AUTO_TEST_CASE(just_a_square) {
  multi_polygon_type mp;
  polygon_type new_poly;
  bg::read_wkt("POLYGON((0 0, 0 100, 100 100, 100 0, 0 0))", new_poly);
  mp.push_back(new_poly);
  print_svg(mp, "just_a_square_input.svg");
  box_type bounding_box;
  bg::envelope(mp, bounding_box);
  const auto& result = Voronoi::build_voronoi(mp, bounding_box, 10);
  print_svg(result, "just_a_square_output.svg");
  BOOST_CHECK(result.size() == 1);
}

BOOST_AUTO_TEST_CASE(square_with_hole) {
  multi_polygon_type mp;
  polygon_type new_poly;
  bg::read_wkt("POLYGON((0 0, 0 100, 100 100, 100 0, 0 0),(50 20, 80 50, 50 80, 20 50, 50 20))", new_poly);
  mp.push_back(new_poly);
  new_poly.clear();
  bg::read_wkt("POLYGON((45 45, 45 55, 55 55, 55 45, 45 45))", new_poly);
  mp.push_back(new_poly);
  print_svg(mp, "square_with_hole_input.svg");
  box_type bounding_box;
  bg::envelope(mp, bounding_box);
  const auto& result = Voronoi::build_voronoi(mp, bounding_box, 10);
  print_svg(result, "square_with_hole_output.svg");
  BOOST_CHECK(result.size() == 2);
  BOOST_CHECK(result[0].inners().size() == 1);
  BOOST_CHECK(result[1].inners().size() == 0);
}

BOOST_AUTO_TEST_CASE(almost_square_with_hole) {
  multi_polygon_type mp;
  polygon_type new_poly;
  bg::read_wkt("POLYGON((0 0, 0 100, 100 100, 100 55, 80 55, 50 80, 20 50, 50 20, 100 45, 100 0, 0 0))", new_poly);
  mp.push_back(new_poly);
  new_poly.clear();
  bg::read_wkt("POLYGON((45 45, 45 55, 55 55, 55 45, 45 45))", new_poly);
  mp.push_back(new_poly);
  print_svg(mp, "square_with_hole_input.svg");
  box_type bounding_box;
  bg::envelope(mp, bounding_box);
  const auto& result = Voronoi::build_voronoi(mp, bounding_box, 10);
  print_svg(result, "square_with_hole_output.svg");
  BOOST_CHECK(result.size() == 2);
  BOOST_CHECK(result[0].inners().size() == 1);
  BOOST_CHECK(result[1].inners().size() == 0);
}

BOOST_AUTO_TEST_SUITE_END()
