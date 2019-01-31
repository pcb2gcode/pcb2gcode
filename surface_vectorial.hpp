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
#include <algorithm>

#include <fstream>

#include <memory>

#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>

#include "mill.hpp"
#include "gerberimporter.hpp"
#include "core.hpp"
#include "voronoi.hpp"
#include "units.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class Surface_vectorial: public Core, virtual public boost::noncopyable {
 public:
  Surface_vectorial(unsigned int points_per_circle, ivalue_t width, ivalue_t height,
                    string name, string outputdir, bool tsp_2opt, MillFeedDirection::MillFeedDirection mill_feed_direction);

  std::vector<std::vector<std::shared_ptr<icoords>>> get_toolpath(
      std::shared_ptr<RoutingMill> mill, bool mirror);
  void save_debug_image(string message);
  void enable_filling();
  void add_mask(std::shared_ptr<Core> surface);
  void render(std::shared_ptr<VectorialLayerImporter> importer);

  inline ivalue_t get_width_in() {
    return width_in;
  }

  inline ivalue_t get_height_in() {
    return height_in;
  }

protected:
  const unsigned int points_per_circle;
  const ivalue_t width_in;
  const ivalue_t height_in;
  const string name;
  const string outputdir;
  const bool tsp_2opt;
  static unsigned int debug_image_index;

  bool fill;
  const MillFeedDirection::MillFeedDirection mill_feed_direction;

  std::shared_ptr<multi_polygon_type_fp> vectorial_surface;
  multi_polygon_type_fp voronoi;
  vector<ring_type_fp> thermal_holes;

  coordinate_type_fp scale;
  box_type_fp bounding_box;

  std::shared_ptr<Surface_vectorial> mask;

  vector<multi_linestring_type_fp> get_single_toolpath(
      std::shared_ptr<RoutingMill> mill, bool mirror, const double tool_diameter, const double overlap_width, const std::string& tool_suffix,
      const multi_polygon_type_fp& already_milled) const;
  std::vector<multi_polygon_type_fp> offset_polygon(
      const boost::optional<polygon_type_fp>& input,
      const polygon_type_fp& voronoi,
      bool& contentions, coordinate_type_fp scaled_diameter,
      coordinate_type_fp scaled_overlap,
      unsigned int steps, bool do_voronoi) const;
  void post_process_toolpath(const std::shared_ptr<RoutingMill>& mill, multi_linestring_type_fp& toolpath) const;
};

class svg_writer
{
public:
    svg_writer(string filename, coordinate_type_fp scale, box_type_fp bounding_box);
    template <typename multi_polygon_type_t>
    void add(const multi_polygon_type_t& geometry, double opacity, bool stroke);
    void add(const std::vector<polygon_type_fp>& geometries, double opacity,
        int r = -1, int g = -1, int b = -1);

protected:
    std::ofstream output_file;
    box_type_fp bounding_box;
    unique_ptr<bg::svg_mapper<point_type_fp> > mapper;
};

#endif // SURFACE_VECTORIAL_H
