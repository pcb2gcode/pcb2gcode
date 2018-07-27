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

#include <fstream>
#include <limits>
using std::numeric_limits;

#include <iostream>
using std::cerr;
using std::endl;

#include <boost/format.hpp>
#include <boost/optional.hpp>

#include <glibmm/miscutils.h>
using Glib::build_filename;

#include "tsp_solver.hpp"
#include "surface_vectorial.hpp"
#include "eulerian_paths.hpp"
#include "segmentize.hpp"
#include "bg_helpers.hpp"

using std::max;
using std::max_element;
using std::next;

unsigned int Surface_vectorial::debug_image_index = 0;

Surface_vectorial::Surface_vectorial(unsigned int points_per_circle, ivalue_t width,
                                     ivalue_t height, string name, string outputdir,
                                     bool tsp_2opt) :
    points_per_circle(points_per_circle),
    width_in(width),
    height_in(height),
    name(name),
    outputdir(outputdir),
    tsp_2opt(tsp_2opt),
    fill(false) {}

void Surface_vectorial::render(shared_ptr<VectorialLayerImporter> importer)
{
    unique_ptr<multi_polygon_type> vectorial_surface_not_simplified;

    vectorial_surface = make_shared<multi_polygon_type_fp>();
    vectorial_surface_not_simplified = importer->render(fill, points_per_circle);
    multi_polygon_type_fp vectorial_surface_not_simplified_fp;
    bg::convert(*vectorial_surface_not_simplified, vectorial_surface_not_simplified_fp);

    if (bg::intersects(vectorial_surface_not_simplified_fp))
    {
        cerr << "\nWarning: Geometry of layer '" << name << "' is"
             << " self-intersecting. This can cause pcb2gcode to produce"
             << " wildly incorrect toolpaths. You may want to check the"
             << " g-code output and/or fix your gerber files!\n";
    }

    scale = importer->vectorial_scale();

    //With a very small loss of precision we can reduce memory usage and processing time
    bg::simplify(vectorial_surface_not_simplified_fp, *vectorial_surface, scale / 10000);
    bg::envelope(*vectorial_surface, bounding_box);
}

vector<shared_ptr<icoords>> Surface_vectorial::get_toolpath(shared_ptr<RoutingMill> mill,
        bool mirror) {
    multi_polygon_type_fp voronoi;
    coordinate_type_fp tolerance = mill->tolerance * scale;
    // This is by how much we will grow each trace if extra passes are needed.
    coordinate_type_fp grow = mill->tool_diameter / 2 * scale;

    shared_ptr<Isolator> isolator = dynamic_pointer_cast<Isolator>(mill);
    // Extra passes are done on each trace if requested, each offset by half the tool diameter.
    const int extra_passes = isolator ? isolator->extra_passes : 0;
    const bool do_voronoi = isolator ? isolator->voronoi : false;

    if (tolerance <= 0)
        tolerance = 0.0001 * scale;

    if (isolator && isolator->preserve_thermal_reliefs && do_voronoi) {
        preserve_thermal_reliefs(*vectorial_surface, std::max(grow, tolerance));
    }

    bg::unique(*vectorial_surface);
    box_type voronoi_bounding_box;
    bg::convert(bounding_box, voronoi_bounding_box);
    multi_polygon_type voronoi_vectorial_surface;
    bg::convert(*vectorial_surface, voronoi_vectorial_surface);
    voronoi = Voronoi::build_voronoi(voronoi_vectorial_surface, voronoi_bounding_box, tolerance);

    box_type_fp svg_bounding_box;

    if (grow > 0)
        bg::buffer(bounding_box, svg_bounding_box, grow * (extra_passes + 1));
    else
        bg::convert(bounding_box, svg_bounding_box);

    const string traced_filename = (boost::format("outp%d_traced_%s.svg") % debug_image_index++ % name).str();
    svg_writer debug_image(build_filename(outputdir, "processed_" + name + ".svg"), SVG_PIX_PER_IN, scale, svg_bounding_box);
    svg_writer traced_debug_image(build_filename(outputdir, traced_filename), SVG_PIX_PER_IN, scale, svg_bounding_box);

    srand(1);
    debug_image.add(voronoi, 0.3, false);

    bool contentions = false;

    srand(1);
    multi_linestring_type_fp toolpath;

    for (unsigned int i = 0; i < vectorial_surface->size(); i++)
    {
        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;

        vector<multi_polygon_type_fp> polygons;
        polygons = offset_polygon(vectorial_surface->at(i), voronoi[i], contentions,
                                  grow, extra_passes + 1, do_voronoi);
        for (multi_polygon_type_fp polygon : polygons) {
          attach_polygons(polygon, toolpath, grow*2);
          debug_image.add(polygon, 0.6, r, g, b);
          traced_debug_image.add(polygon, 1, r, g, b);
        }
        // The polygon is made of rings.  We want to look for rings such that
        // one is entirely inside the other and they have a spot where the
        // distance between them is less than the width of the milling tool.
        // Those are rings that we can mill in a single plunge without lifting
        // the tool.

    }

    srand(1);
    debug_image.add(*vectorial_surface, 1, true);

    if (contentions)
    {
        cerr << "\nWarning: pcb2gcode hasn't been able to fulfill all"
             << " clearance requirements and tried a best effort approach"
             << " instead. You may want to check the g-code output and"
             << " possibly use a smaller milling width.\n";
    }

    if (mill->eulerian_paths) {
        toolpath = eulerian_paths(toolpath);
    }
    if (tsp_2opt) {
        tsp_solver::tsp_2opt( toolpath, point_type_fp(0, 0) );
    } else {
        tsp_solver::nearest_neighbour( toolpath, point_type_fp(0, 0) );
    }
    auto scaled_toolpath = scale_and_mirror_toolpath(toolpath, mirror);
    if (mill->optimise)
    {
        vector<shared_ptr<icoords> > toolpath_optimised;
        for (const shared_ptr<icoords>& ring : scaled_toolpath)
        {
            toolpath_optimised.push_back(make_shared<icoords>());
            bg::simplify(*ring, *(toolpath_optimised.back()), mill->tolerance);
        }

        return toolpath_optimised;
    }
    else
        return scaled_toolpath;
}

void Surface_vectorial::save_debug_image(string message)
{
    const string filename = (boost::format("outp%d_%s.svg") % debug_image_index % message).str();
    svg_writer debug_image(build_filename(outputdir, filename), SVG_PIX_PER_IN, scale, bounding_box);

    srand(1);
    debug_image.add(*vectorial_surface, 1, true);

    ++debug_image_index;
}

void Surface_vectorial::enable_filling()
{
    fill = true;
}

void Surface_vectorial::add_mask(shared_ptr<Core> surface)
{
    mask = dynamic_pointer_cast<Surface_vectorial>(surface);

    if (mask)
    {
        auto masked_surface = make_shared<multi_polygon_type_fp>();

        bg::intersection(*vectorial_surface, *(mask->vectorial_surface), *masked_surface);
        vectorial_surface = masked_surface;

        bg::envelope(*(mask->vectorial_surface), bounding_box);
    }
    else
        throw std::logic_error("Can't cast Core to Surface_vectorial");
}

vector<shared_ptr<icoords>> Surface_vectorial::scale_and_mirror_toolpath(
    const multi_linestring_type_fp& mls, bool mirror) {
    vector<shared_ptr<icoords>> result;
    for (const auto& ls : mls) {
        icoords coords;
        for (const auto& point : ls) {
            if (mirror) {
                coords.push_back(make_pair((-point.x()) / double(scale),
                                           point.y() / double(scale)));
            } else {
                coords.push_back(make_pair(point.x() / double(scale),
                                           point.y() / double(scale)));
            }
        }
        result.push_back(make_shared<icoords>(coords));
    }
    return result;
}

vector<multi_polygon_type_fp> Surface_vectorial::offset_polygon(
    const polygon_type_fp& input,
    const polygon_type_fp& voronoi_polygon,
    bool& contentions, coordinate_type_fp offset,
    unsigned int steps, bool do_voronoi) {
  // The polygons to add to the PNG debuging output files.
  vector<multi_polygon_type_fp> polygons;

  // Mask the polygon that we need to mill.
  polygon_type_fp masked_milling_poly = do_voronoi ? voronoi_polygon : input;  // Milling voronoi or trace?
  multi_polygon_type_fp masked_milling_polys;
  if (mask) {
    bg::intersection(masked_milling_poly, *(mask->vectorial_surface), masked_milling_polys);
  } else {
    bg::intersection(masked_milling_poly, bounding_box, masked_milling_polys);
  }

  // Convert the input shape into a bunch of rings that need to be milled.
  for (unsigned int i = 0; i < steps; i++) {
    coordinate_type_fp expand_by;
    if (!do_voronoi) {
      // Number of rings is the same as the number of steps.
      expand_by = offset * (i+1);
    } else {
      // Voronoi lines are on the boundary and shared between
      // multi_polygons so we only need half as many of them.
      double factor = ((1-double(steps))/2 + i);
      if (factor > 0) {
        continue; // Don't need this step.
      }
      expand_by = offset * factor;
    }

    if (expand_by == 0) {
      // We simply need to mill every ring in the shape.
      polygons.push_back(masked_milling_polys);
    } else {
      multi_polygon_type_fp mpoly_temp;
      // Buffer should be done on floating point polygons.
      bg_helpers::buffer(masked_milling_polys, mpoly_temp, expand_by);

      multi_polygon_type_fp mpoly;
      if (!do_voronoi) {
        bg::intersection(mpoly_temp, voronoi_polygon, mpoly);
      } else {
        bg::union_(mpoly_temp, input, mpoly);
      }
      polygons.push_back(mpoly);
      if (!bg::equals(mpoly_temp, mpoly)) {
        contentions = true;
      }
    }
  }

  return polygons;
}

// Given a ring, attach it to one of the ends of the toolpath.  Only attach if
// there is a point on the ring that is close enough to the toolpath endpoint.
// toolpath must not be empty.
bool Surface_vectorial::attach_ring(const ring_type_fp& ring, linestring_type_fp& toolpath, const coordinate_type_fp& max_distance) {
  bool insert_at_front = true;
  auto best_ring_point = ring.begin();
  double best_distance = bg::comparable_distance(*best_ring_point, toolpath.front());
  for (auto ring_point = ring.begin(); ring_point != ring.end(); ring_point++) {
    if (bg::comparable_distance(*ring_point, toolpath.front()) < best_distance) {
      best_distance = bg::comparable_distance(*ring_point, toolpath.front());
      best_ring_point = ring_point;
      insert_at_front = true;
    }
    if (bg::comparable_distance(*ring_point, toolpath.back()) < best_distance) {
      best_distance = bg::comparable_distance(*ring_point, toolpath.back());
      best_ring_point = ring_point;
      insert_at_front = false;
    }
  }
  if (bg::distance(*best_ring_point,
                   insert_at_front ? toolpath.front() : toolpath.back()) >= max_distance) {
    return false;
  }
  toolpath.resize(toolpath.size() + ring.size()); // Make space for the ring.
  auto insertion_point = toolpath.end() - ring.size(); // Insert at the end
  if (insert_at_front) {
    std::move_backward(toolpath.begin(), insertion_point, toolpath.end());
    insertion_point = toolpath.begin();
  }
  auto close_ring_point = std::rotate_copy(ring.begin(), best_ring_point, std::prev(ring.end()), insertion_point);
  *close_ring_point = *best_ring_point;
  return true;
}

// Given a ring, attach it to one of the toolpaths.  Only attach if there is a
// point on the ring that is close enough to one of the toolpaths' endpoints.
// If none of the toolpaths have a close enough endpint, a new toolpath is added
// to the list of toolpaths.
void Surface_vectorial::attach_ring(const ring_type_fp& ring, multi_linestring_type_fp& toolpaths,
                                    const coordinate_type_fp& max_distance) {
  for (auto& toolpath : toolpaths) {
    if (attach_ring(ring, toolpath, max_distance)) {
      return;
    }
  }
  toolpaths.push_back(linestring_type_fp(ring.begin(), ring.end()));
}

// Given polygons, attach all the rings inside to the toolpaths.
void Surface_vectorial::attach_polygons(const multi_polygon_type_fp& polygons, multi_linestring_type_fp& toolpaths,
                                        const coordinate_type_fp& max_distance) {
  // Loop through the polygons by ring index because that will lead to better
  // connections between loops.
  for (const auto& poly : polygons) {
    attach_ring(poly.outer(), toolpaths, max_distance);
  }
  bool found_one = true;
  for (size_t i = 0; found_one; i++) {
    found_one = false;
    for (const auto& poly : polygons) {
      if (poly.inners().size() > i) {
        found_one = true;
        attach_ring(poly.inners()[i], toolpaths, max_distance);
      }
    }
  }
}

size_t Surface_vectorial::merge_near_points(multi_linestring_type_fp& mls) {
    struct PointLessThan {
        bool operator()(const point_type_fp& a, const point_type_fp& b) const {
            return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
        }
    };

    std::map<point_type_fp, point_type_fp, PointLessThan> points;
    for (const auto& ls : mls) {
        for (const auto& point : ls) {
            points[point] = point;
        }
    }
    // Merge points that are near one another.  This doesn't do a
    // great job but it's fast enough.
    size_t points_merged = 0;
    for (auto i = points.begin(); i != points.end(); i++) {
        for (auto j = i;
             j != points.upper_bound(point_type_fp(i->second.x()+10,
                                                i->second.y()+10));
             j++) {
            if (!bg::equals(j->second, i->second) &&
                bg::comparable_distance(i->second, j->second) <= 100) {
                points_merged++;
                j->second = i->second;
            }
        }
    }
    if (points_merged > 0) {
        for (auto& ls : mls) {
            for (auto& point : ls) {
                point = points[point];
            }
        }
    }
    return points_merged;
}

multi_linestring_type_fp Surface_vectorial::eulerian_paths(const multi_linestring_type_fp& toolpaths) {
    // Merge points that are very close to each other because it makes
    // us more likely to find intersections that was can use.
    multi_linestring_type_fp merged_toolpaths(toolpaths);
    merge_near_points(merged_toolpaths);

    // First we need to split all paths so that they don't cross.
    vector<segment_type_p> all_segments;
    for (const auto& toolpath : merged_toolpaths) {
        for (size_t i = 1; i < toolpath.size(); i++) {
            all_segments.push_back(
                segment_type_p(
                    point_type_p(toolpath[i-1].x(), toolpath[i-1].y()),
                    point_type_p(toolpath[i  ].x(), toolpath[i  ].y())));
        }
    }
    vector<segment_type_p> split_segments = segmentize(all_segments);

    multi_linestring_type_fp segments_as_linestrings;
    for (const auto& segment : split_segments) {
        // Make a little 1-edge linestrings, filter out those that
        // aren't in the mask.
        linestring_type_fp ls;
        ls.push_back(point_type_fp(segment.low().x(), segment.low().y()));
        ls.push_back(point_type_fp(segment.high().x(), segment.high().y()));
        segments_as_linestrings.push_back(ls);
    }

    // Make a minimal number of paths from those segments.
    struct PointLessThan {
      bool operator()(const point_type_fp& a, const point_type_fp& b) const {
          return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
      }
    };

    return eulerian_paths::get_eulerian_paths<
      point_type_fp,
      linestring_type_fp,
      multi_linestring_type_fp,
      PointLessThan>(segments_as_linestrings);
}

size_t Surface_vectorial::preserve_thermal_reliefs(multi_polygon_type_fp& milling_surface, const coordinate_type_fp& grow) {
    // For each shape, see if it has any holes that are empty.
    size_t thermal_reliefs_found = 0;
    boost::optional<svg_writer> image;
    multi_polygon_type_fp holes;
    for (auto& p : milling_surface) {
        for (auto& inner : p.inners()) {
            auto thermal_hole = inner;
            bg::correct(thermal_hole); // Convert it from a hole to a filled-in shape.
            multi_polygon_type_fp shrunk_thermal_hole;
            bg_helpers::buffer(thermal_hole, shrunk_thermal_hole, -grow);
            bool empty_hole = !bg::intersects(shrunk_thermal_hole, milling_surface);
            if (empty_hole) {
                thermal_reliefs_found++;
                if (!image) {
                    image.emplace(build_filename(outputdir, "thermal_reliefs_" + name + ".svg"), SVG_PIX_PER_IN, scale, bounding_box);
                }
                image->add(shrunk_thermal_hole, 1, true);
                for (const auto& p : shrunk_thermal_hole) {
                    holes.push_back(p);
                }
            }
        }
    }
    milling_surface.insert(milling_surface.end(), holes.begin(), holes.end());
    return thermal_reliefs_found;
}

svg_writer::svg_writer(string filename, unsigned int pixel_per_in, coordinate_type_fp scale, box_type_fp bounding_box) :
    output_file(filename),
    bounding_box(bounding_box)
{
    const coordinate_type_fp width =
        (bounding_box.max_corner().x() - bounding_box.min_corner().x()) * pixel_per_in / scale;
    const coordinate_type_fp height =
        (bounding_box.max_corner().y() - bounding_box.min_corner().y()) * pixel_per_in / scale;

    //Some SVG readers does not behave well when viewBox is not specified
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"0 0 %1% %2%\"") % width % height);

    mapper = unique_ptr<bg::svg_mapper<point_type_fp> >
        (new bg::svg_mapper<point_type_fp>(output_file, width, height, svg_dimensions));
    mapper->add(bounding_box);
}

template <typename multi_polygon_type_t>
void svg_writer::add(const multi_polygon_type_t& geometry, double opacity, bool stroke)
{
    string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:2" : "";

    for (const auto& poly : geometry)
    {
        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;
        multi_polygon_type_t mpoly;

        multi_polygon_type_t new_bounding_box;
        bg::convert(bounding_box, new_bounding_box);
        bg::intersection(poly, new_bounding_box, mpoly);

        mapper->map(mpoly,
            str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);" + stroke_str) %
            opacity % r % g % b));
    }
}

void svg_writer::add(const vector<polygon_type_fp>& geometries, double opacity, int r, int g, int b)
{
    if (r < 0 || g < 0 || b < 0)
    {
        r = rand() % 256;
        g = rand() % 256;
        b = rand() % 256;
    }

    for (unsigned int i = geometries.size(); i != 0; i--)
    {
        multi_polygon_type_fp mpoly;

        bg::intersection(geometries[i - 1], bounding_box, mpoly);

        if (i == geometries.size())
        {
            mapper->map(mpoly,
                str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);stroke:rgb(0,0,0);stroke-width:2") %
                opacity % r % g % b));
        }
        else
        {
            mapper->map(mpoly, "fill:none;stroke:rgb(0,0,0);stroke-width:1");
        }
    }
}

