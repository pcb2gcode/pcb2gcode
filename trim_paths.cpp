#include "geometry.hpp"
#include "bg_helpers.hpp"

#include "trim_paths.hpp"

namespace trim_paths {

using std::pair;
using std::vector;
using std::multiset;
using std::reverse;
using std::remove_if;

// Returns true if the segment described by the two points is in the
// haystack.  Finds reversed segments if the segment is reversible.
// Prefers directional, however.  Removes the found element or returns
// false.
bool segment_in_path(const point_type_fp& start, const point_type_fp& end, multiset<pair<linestring_type_fp, bool>>& haystack) {
  auto to_delete = haystack.find({{start, end}, false});
  if (to_delete != haystack.cend()) {
    haystack.erase(to_delete);
    return true;
  }
  to_delete = haystack.find({{start, end}, true});
  if (to_delete != haystack.cend()) {
    haystack.erase(to_delete);
    return true;
  }
  to_delete = haystack.find({{end, start}, true});
  if (to_delete != haystack.cend()) {
    haystack.erase(to_delete);
    return true;
  }
  return false;
}

void trim_path(pair<linestring_type_fp, bool>& ls,
               multiset<pair<linestring_type_fp, bool>>& backtracks) {
  if (ls.first.size() < 2) {
    return; // Nothing to remove.
  }
  // backtracks not yet seen.
  multiset<pair<linestring_type_fp, bool>> backtracks_multiset;
  for (const auto& p : backtracks) {
    backtracks_multiset.insert(p);
  }
  // First check for how much can be removed from the start.
  // This needs to point to one beyond the end of the points to
  // remove.
  auto remove_from_start = ls.first.cbegin();
  double length_from_start = 0;
  for (auto current = ls.first.cbegin(); current+1 != ls.first.cend(); current++) {
    if (segment_in_path(*current, *(current+1), backtracks_multiset)) {
      remove_from_start = current + 1;
      length_from_start += bg::distance(*current, *(current+1));
    } else {
      break;
    }
  }
  // Now check for how much can be removed from the end.  This
  // needs to point to the first vertex to remove.
  auto remove_from_end = ls.first.cend();
  double length_from_end = 0;
  for (auto current = remove_from_end - 1; current != ls.first.begin(); current--) {
    if (segment_in_path(*(current-1), *current, backtracks_multiset)) {
      remove_from_end = current;
      length_from_end += bg::distance(*(current-1), *current);
    } else {
      break;
    }
  }

  double longest_so_far = 0;
  auto longest_start = ls.first.cbegin();
  auto longest_end = ls.first.cbegin();
  if (ls.first.front() == ls.first.back()) {
    // For loops, see if we can do better by removing parts of the middle.
    for (auto current = ls.first.cbegin(); current+1 != ls.first.cend();) {
      backtracks_multiset.clear();
      for (const auto& p : backtracks) {
        backtracks_multiset.insert(p);
      }
      while (current + 1 != ls.first.cend() && !segment_in_path(*current, *(current+1), backtracks_multiset)) {
        current++;
      }
      if (current + 1 == ls.first.cend()) {
        break;
      }
      double current_length = bg::distance(*current, *(current + 1));
      auto current_start = current; // First vertex in backtrack.
      auto current_end = current + 1; // Last vertex in backtrack.
      for (current++; current + 1 != ls.first.cend() && segment_in_path(*current, *(current+1), backtracks_multiset); current++) {
        current_end = current + 1;
        current_length += bg::distance(*current, *(current + 1));
      }
      // How long did we find?
      if (current_length > longest_so_far) {
        longest_so_far = current_length;
        longest_start = current_start;
        longest_end = current_end;
      }
    }
  }
  // Delete that longest bit,
  if (length_from_start + length_from_end > longest_so_far) {
    // Update the caller's backtracks.
    for (auto current = remove_from_end - 1; current + 1 != ls.first.cend(); current++) {
      segment_in_path(*current, *(current+1), backtracks);
    }
    for (auto current = ls.first.cbegin(); current != remove_from_start; current++) {
      segment_in_path(*current, *(current+1), backtracks);
    }
    // Just delete from the start and from the end.
    ls.first.erase(remove_from_end, ls.first.cend());
    ls.first.erase(ls.first.cbegin(), remove_from_start);
  } else {
    // Update the caller's backtracks.
    for (auto current = longest_start; current != longest_end; current++) {
      segment_in_path(*current, *(current+1), backtracks);
    }
    // This is loop and we found a middle section to remove.
    linestring_type_fp new_ls;
    new_ls.insert(new_ls.cend(), longest_end, ls.first.cend());
    new_ls.insert(new_ls.cend(), ls.first.cbegin() + 1, longest_start+1);
    ls.first = new_ls;
  }
}

// Given toolpaths and backtracks, look for segments in toolspaths
// that match backtracks and remove them.  This makes the toolpaths
// smaller.  The backtracks are expected to be stright segments with
// just two vertices.
void trim_paths(vector<pair<linestring_type_fp, bool>>& toolpaths,
                const vector<pair<linestring_type_fp, bool>>& backtracks) {
  if (backtracks.size() == 0) {
    return;
  }
  // backtrack adds enough paths to make a eulerian circuit but we
  // just need a eulerian path, so find the longest stretch of
  // backtracks and remove those.
  multiset<pair<linestring_type_fp, bool>> bt{backtracks.cbegin(), backtracks.cend()};
  for (auto& ls : toolpaths) {
    trim_path(ls, bt);
    if (ls.second) {
      auto reverse_ls = ls;
      reverse(reverse_ls.first.begin(), reverse_ls.first.end());
      trim_path(reverse_ls, bt);
      reverse(reverse_ls.first.begin(), reverse_ls.first.end());
      ls = reverse_ls;
    }
  }
  toolpaths.erase(
      remove_if(
          toolpaths.begin(),
          toolpaths.end(),
          [](pair<linestring_type_fp, bool> const& p) {
            return p.first.size() < 2;
          }),
      toolpaths.cend());
}

} // namespace trim_paths
