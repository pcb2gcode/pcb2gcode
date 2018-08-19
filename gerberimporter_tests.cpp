#define BOOST_TEST_MODULE gerberimporter tests
#include <boost/test/included/unit_test.hpp>

#include "gerberimporter.hpp"
#include <sys/types.h>
#include <dirent.h>

BOOST_AUTO_TEST_SUITE(gerberimporter_tests);

BOOST_AUTO_TEST_CASE(all_gerbers) {
  DIR *dirp = opendir("testing/gerberimporter");
  dirent *entry;
  while ((entry = readdir(dirp)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 ||
        strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    // Test this file but for now just print the name.
    printf("name is %s\n", entry->d_name);
  }
  closedir(dirp);
}

BOOST_AUTO_TEST_SUITE_END()
