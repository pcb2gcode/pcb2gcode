/*
 * This file is part of pcb2gcode.
 *
 * Copyright (C) 2016 Nicola Corna <nicola@corna.info>
 *
 * pcb2gcode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pcb2gcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SURFACE_VECTORIAL_H
#define SURFACE_VECTORIAL_H

#include <vector>
#include <list>
#include <forward_list>
#include <map>
#include <utility>
#include <algorithm>

#include <fstream>

#include <memory>

#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>

#include "mill.hpp"
#include "gerberimporter.hpp"
#include "voronoi.hpp"
#include "units.hpp"
#include "path_finding.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class Surface_vectorial: private boost::noncopyable {
 public:
  // This function returns a linestring that connects two points if possible.
  typedef std::function<boost::optional<linestring_type_fp>(const point_type_fp& start, const point_type_fp& end)> PathFinder;
  typedef std::function<boost::optional<linestring_type_fp>(const point_type_fp& start,
                                                            const point_type_fp& end,
                                                            path_finding::SearchKey search_key)> PathFinderRingIndices;

  Surface_vectorial(const box_type_fp& bounding_box,
                    std::string name, std::string outputdir, bool tsp_2opt,
                    MillFeedDirection::MillFeedDirection mill_feed_direction,
                    bool invert_gerbers, bool render_paths_to_shapes);

  std::vector<std::pair<coordinate_type_fp, multi_linestring_type_fp>> get_toolpath(
      std::shared_ptr<RoutingMill> mill, bool mirror, bool ymirror);
  void save_debug_image(std::string message);
  void enable_filling();
  void add_mask(std::shared_ptr<Surface_vectorial> surface);
  // The importer provides the path.  The tolerance is used for
  // removing some of the finer detail in the path, to save time on
  // processing.
  void render(std::shared_ptr<GerberImporter> importer, double tolerance);

  inline coordinate_type_fp get_width_in() {
    return bounding_box.max_corner().x() - bounding_box.min_corner().x();
  }

  inline coordinate_type_fp get_height_in() {
    return bounding_box.max_corner().y() - bounding_box.min_corner().y();
  }

protected:
  const box_type_fp bounding_box;
  const std::string name;
  const std::string outputdir;
  const bool tsp_2opt;
  static unsigned int debug_image_index;

  bool fill;
  const MillFeedDirection::MillFeedDirection mill_feed_direction;
  const bool invert_gerbers;
  const bool render_paths_to_shapes;

  std::shared_ptr<std::pair<multi_polygon_type_fp,
                      std::map<coordinate_type_fp, multi_linestring_type_fp>>>
      vectorial_surface;
  multi_polygon_type_fp voronoi;
  std::vector<polygon_type_fp> thermal_holes;


  std::shared_ptr<Surface_vectorial> mask;

  std::vector<std::pair<linestring_type_fp, bool>> get_single_toolpath(
      std::shared_ptr<RoutingMill> mill, const size_t trace_index, bool mirror, const double tool_diameter,
      const double overlap_width,
      const multi_polygon_type_fp& already_milled,
      const path_finding::PathFindingSurface& path_finding_surface) const;
  PathFinder make_path_finder(
      std::shared_ptr<RoutingMill> mill,
      const path_finding::PathFindingSurface& path_finding_surface) const;
  PathFinderRingIndices make_path_finder_ring_indices(
      std::shared_ptr<RoutingMill> mill,
      const path_finding::PathFindingSurface& path_finding_surface) const;
  std::vector<std::pair<linestring_type_fp, bool>> final_path_finder(
      const std::shared_ptr<RoutingMill>& mill,
      const path_finding::PathFindingSurface& path_finding_surface,
      const std::vector<std::pair<linestring_type_fp, bool>>& paths) const;
  std::vector<multi_polygon_type_fp> offset_polygon(
      const boost::optional<polygon_type_fp>& input,
      const polygon_type_fp& voronoi,
      coordinate_type_fp diameter,
      coordinate_type_fp overlap,
      unsigned int steps, bool do_voronoi,
      coordinate_type_fp offset) const;
  multi_linestring_type_fp post_process_toolpath(
      const std::shared_ptr<RoutingMill>& mill,
      const boost::optional<const path_finding::PathFindingSurface*>& path_finding_surface,
      std::vector<std::pair<linestring_type_fp, bool>> toolpath) const;
  void write_svgs(const std::string& tool_suffix, coordinate_type_fp tool_diameter,
                  const std::vector<std::vector<std::pair<linestring_type_fp, bool>>>& new_trace_toolpaths,
                  coordinate_type_fp tolerance, bool find_contentions) const;
  void write_svgs(const std::string& tool_suffix, coordinate_type_fp tool_diameter,
                  const multi_linestring_type_fp& toolpaths,
                  coordinate_type_fp tolerance, bool find_contentions) const;
};

#endif // SURFACE_VECTORIAL_H
