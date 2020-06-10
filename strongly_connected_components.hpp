#ifndef STRONGLY_CONNECTED_COMPONENTS_HPP
#define STRONGLY_CONNECTED_COMPONENTS_HPP

namespace strongly_connected_components {

// Given a graph of edges where some edges might be directed and some
// not and there may be edges which are loops, return the vertices of
// the edges as a list of list of vertices.
std::vector<std::vector<point_type_fp>> strongly_connected_components(
    const std::vector<std::pair<linestring_type_fp, bool>>& paths);

} // namespace strongly_connected_components

#endif //STRONGLY_CONNECTED_COMPONENTS_HPP
