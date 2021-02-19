#define BOOST_TEST_MODULE segment tree tests
#include <boost/test/unit_test.hpp>

#include "segment_tree.hpp"

using namespace std;
using namespace segment_tree;

BOOST_AUTO_TEST_SUITE(segment_tree_tests)

BOOST_AUTO_TEST_CASE(make_node) {
  vector<std::pair<point_type_fp, point_type_fp>> segments{{{0,15}, {100,15}},
                                                           {{5,0}, {5,0}},
                                                           {{4,12}, {9,6}},
                                                           {{2,14}, {5,18}},
                                                           {{9,9}, {9,9}},
  };
  auto tree = SegmentTree(segments);
  tree.print();
}
BOOST_AUTO_TEST_SUITE_END()
