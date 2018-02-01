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

#include <glibmm/miscutils.h>
using Glib::build_filename;

#include "tsp_solver.hpp"
#include "surface_vectorial.hpp"
#include "eulerian_paths.hpp"
#include "segmentize.hpp"

using std::max;
using std::max_element;
using std::next;

unsigned int Surface_vectorial::debug_image_index = 0;

Surface_vectorial::Surface_vectorial(unsigned int points_per_circle, ivalue_t width,
                                        ivalue_t height, string name, string outputdir) :
    points_per_circle(points_per_circle),
    width_in(width),
    height_in(height),
    name(name),
    outputdir(outputdir),
    fill(false)
{

}

void Surface_vectorial::render(shared_ptr<VectorialLayerImporter> importer)
{
    unique_ptr<multi_polygon_type> vectorial_surface_not_simplified;

    vectorial_surface = make_shared<multi_polygon_type>();
    vectorial_surface_not_simplified = importer->render(fill, points_per_circle);

    if (bg::intersects(*vectorial_surface_not_simplified))
    {
        cerr << "\nWarning: Geometry of layer '" << name << "' is"
             << " self-intersecting. This can cause pcb2gcode to produce"
             << " wildly incorrect toolpaths. You may want to check the"
             << " g-code output and/or fix your gerber files!\n";
    }

    scale = importer->vectorial_scale();

    //With a very small loss of precision we can reduce memory usage and processing time
    bg::simplify(*vectorial_surface_not_simplified, *vectorial_surface, scale / 10000);
    bg::envelope(*vectorial_surface, bounding_box);
}

vector<shared_ptr<icoords> > Surface_vectorial::get_toolpath(shared_ptr<RoutingMill> mill,
        bool mirror)
{
    multi_linestring_type toolpath;
    vector<shared_ptr<icoords> > toolpath_optimised;
    multi_polygon_type_fp voronoi;
    coordinate_type tolerance = mill->tolerance * scale;
    // This is by how much we will grow each trace if extra passes are needed.
    coordinate_type grow = mill->tool_diameter / 2 * scale;

    shared_ptr<Isolator> isolator = dynamic_pointer_cast<Isolator>(mill);
    // Extra passes are done on each trace if requested, each offset by half the tool diameter.
    const int extra_passes = isolator ? isolator->extra_passes : 0;
    const bool do_voronoi = isolator ? isolator->voronoi : false;

    if (tolerance <= 0)
        tolerance = 0.0001 * scale;

    bg::unique(*vectorial_surface);
    box_type voronoi_bounding_box;
    bg::convert(bounding_box, voronoi_bounding_box);
    voronoi = Voronoi::build_voronoi(*vectorial_surface, voronoi_bounding_box, tolerance);
    multi_polygon_type integral_voronoi;
    bg::convert(voronoi, integral_voronoi);

    box_type svg_bounding_box;

    if (grow > 0)
        bg::buffer(bounding_box, svg_bounding_box, grow * (extra_passes + 1));
    else
        bg::assign(svg_bounding_box, bounding_box);

    const string traced_filename = (boost::format("outp%d_traced_%s.svg") % debug_image_index++ % name).str();
    svg_writer debug_image(build_filename(outputdir, "processed_" + name + ".svg"), SVG_PIX_PER_IN, scale, svg_bounding_box);
    svg_writer traced_debug_image(build_filename(outputdir, traced_filename), SVG_PIX_PER_IN, scale, svg_bounding_box);

    srand(1);
    debug_image.add(integral_voronoi, 0.3, false);

    const coordinate_type mirror_axis = mill->mirror_absolute ?
        bounding_box.min_corner().x() :
        ((bounding_box.min_corner().x() + bounding_box.max_corner().x()) / 2);
    bool contentions = false;

    srand(1);

    for (unsigned int i = 0; i < vectorial_surface->size(); i++)
    {
        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;

        unique_ptr<vector<polygon_type> > polygons;
    
        polygons = offset_polygon(*vectorial_surface, integral_voronoi, toolpath, contentions,
                                  grow, i, extra_passes + 1, do_voronoi);

        debug_image.add(*polygons, 0.6, r, g, b);
        traced_debug_image.add(*polygons, 1, r, g, b);
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
    tsp_solver::nearest_neighbour( toolpath, point_type(0, 0), 0.0001 );
    auto scaled_toolpath = scale_and_mirror_toolpath(toolpath, mirror, mirror_axis);
    if (mill->optimise)
    {
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
        auto masked_surface = make_shared<multi_polygon_type>();

        bg::intersection(*vectorial_surface, *(mask->vectorial_surface), *masked_surface);
        vectorial_surface = masked_surface;

        bg::envelope(*(mask->vectorial_surface), bounding_box);
    }
    else
        throw std::logic_error("Can't cast Core to Surface_vectorial");
}

vector<shared_ptr<icoords>> Surface_vectorial::scale_and_mirror_toolpath(
    const multi_linestring_type& mls, bool mirror, ivalue_t mirror_axis) {
    vector<shared_ptr<icoords>> result;
    for (const auto& ls : mls) {
        icoords coords;
        for (const auto& point : ls) {
            if (mirror) {
                coords.push_back(make_pair((2 * mirror_axis - point.x()) / double(scale),
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

unique_ptr<vector<polygon_type> > Surface_vectorial::offset_polygon(const multi_polygon_type& input,
                            const multi_polygon_type& voronoi_polygons, multi_linestring_type& toolpath,
                            bool& contentions, coordinate_type offset, size_t index,
                            unsigned int steps, bool do_voronoi)
{
    unique_ptr<vector<polygon_type> > polygons (new vector<polygon_type>(steps));
    list<list<const ring_type *> > rings (steps);
    auto ring_i = rings.begin();
    point_type last_point;

    auto push_point = [&](const point_type& point)
    {
        toolpath.back().push_back(point);
    };

    auto copy_ring_to_toolpath = [&](const ring_type& ring, unsigned int start)
    {
        const auto size_minus_1 = ring.size() - 1;
        unsigned int i = start;

        do
        {
            push_point(ring[i]);
            i = (i + 1) % size_minus_1;
        } while (i != start);

        push_point(ring[i]);
        last_point = ring[i];
    };

    auto find_first_nonempty = [&]()
    {
        for (auto i = rings.begin(); i != rings.end(); i++)
            if (!i->empty())
                return i;
        return rings.end();
    };

    auto find_closest_point_index = [&](const ring_type& ring)
    {
        const unsigned int size = ring.size();
        auto min_distance = bg::comparable_distance(ring[0], last_point);
        unsigned int index = 0;

        for (unsigned int i = 1; i < size; i++)
        {
            const auto distance = bg::comparable_distance(ring[i], last_point);

            if (distance < min_distance)
            {
                min_distance = distance;
                index = i;
            }
        }

        return index;
    };

    toolpath.push_back(linestring_type());

    bool outer_collapsed = false;

    for (unsigned int i = 0; i < steps; i++)
    {
        coordinate_type expand_by;
        if (!do_voronoi) {
            // Number of rings is the same as the number of steps.
            expand_by = offset * (i+1);
        } else {
            // Voronoi lines are on the boundary and shared between
            // multi_polygons so we only need half as many of them.
            double factor = ((1-double(steps))/2 + i);
            if (factor > 0) {
                continue; // Don't need it.
            }
            expand_by = offset * factor;
        }
        polygon_type masked_milling_poly = do_voronoi ? voronoi_polygons[index] : input[index];
        multi_polygon_type masked_milling_polys;
        if (mask) {
            bg::intersection(masked_milling_poly, *(mask->vectorial_surface), masked_milling_polys);
        } else {
            bg::convert(masked_milling_poly, masked_milling_polys);
        }
        if (expand_by == 0)
        {
            (*polygons)[i] = masked_milling_polys[0];
        }
        else
        {
            auto mpoly = make_shared<multi_polygon_type>();
            multi_polygon_type mpoly_temp;

            bg::buffer(masked_milling_polys,
                       mpoly_temp,
                       bg::strategy::buffer::distance_symmetric<coordinate_type>(expand_by),
                       bg::strategy::buffer::side_straight(),
                       bg::strategy::buffer::join_round(points_per_circle),
                       //bg::strategy::buffer::join_miter(numeric_limits<coordinate_type>::max()),
                       bg::strategy::buffer::end_flat(),
                       bg::strategy::buffer::point_circle(30));

            bg::intersection(mpoly_temp[0], voronoi_polygons[index], *mpoly);

            (*polygons)[i] = (*mpoly)[0];

            if (!bg::equals((*polygons)[i], mpoly_temp[0]))
                contentions = true;
        }

        if (i == 0)
            copy_ring_to_toolpath((*polygons)[i].outer(), 0);
        else
        {
            if (!outer_collapsed && bg::equals((*polygons)[i].outer(), (*polygons)[i - 1].outer()))
                outer_collapsed = true;

            if (!outer_collapsed)
                copy_ring_to_toolpath((*polygons)[i].outer(), find_closest_point_index((*polygons)[i].outer()));
        }

        for (const ring_type& ring : (*polygons)[i].inners())
            ring_i->push_back(&ring);
        
        ++ring_i;
    }

    ring_i = find_first_nonempty();

    while (ring_i != rings.end())
    {
        const ring_type *biggest = ring_i->front();
        const ring_type *prev = biggest;
        auto ring_j = next(ring_i);

        toolpath.push_back(linestring_type());
        copy_ring_to_toolpath(*biggest, 0);

        while (ring_j != rings.end())
        {
            list<const ring_type *>::iterator j;

            for (j = ring_j->begin(); j != ring_j->end(); j++)
            {
                if (bg::equals(**j, *prev))
                {
                    ring_j->erase(j);
                    break;
                }
                else
                {
                    if (bg::covered_by(**j, *prev))
                    {
                        auto index = find_closest_point_index(**j);
                        ring_type ring (prev->rbegin(), prev->rend());
                        linestring_type segment;

                        segment.push_back((**j)[index]);
                        segment.push_back(last_point);

                        if (bg::covered_by(segment, ring))
                        {
                            copy_ring_to_toolpath(**j, index);
                            prev = *j;
                            ring_j->erase(j);
                            break;
                        }
                    }
                }
            }

            if (j == ring_j->end())
                ring_j = rings.end();
            else
                ++ring_j;
        }

        ring_i->erase(ring_i->begin());
        ring_i = find_first_nonempty();
    }

    return polygons;
}

size_t Surface_vectorial::merge_near_points(multi_linestring_type& mls) {
    struct PointLessThan {
        bool operator()(const point_type& a, const point_type& b) const {
            return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
        }
    };

    std::map<point_type, point_type, PointLessThan> points;
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
             j != points.upper_bound(point_type(i->second.x()+10,
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

multi_linestring_type Surface_vectorial::eulerian_paths(const multi_linestring_type& toolpaths) {
    // Merge points that are very close to each other because it makes
    // us more likely to find intersections that was can use.
    multi_linestring_type merged_toolpaths(toolpaths);
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

    multi_linestring_type segments_as_linestrings;
    for (const auto& segment : split_segments) {
        // Make a little 1-edge linestrings, filter out those that
        // aren't in the mask.
        linestring_type ls;
        ls.push_back(point_type(segment.low().x(), segment.low().y()));
        ls.push_back(point_type(segment.high().x(), segment.high().y()));
        segments_as_linestrings.push_back(ls);
    }

    // Make a minimal number of paths from those segments.
    struct PointLessThan {
      bool operator()(const point_type& a, const point_type& b) const {
          return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
      }
    };

    return eulerian_paths::get_eulerian_paths<
      point_type,
      linestring_type,
      multi_linestring_type,
      PointLessThan>(segments_as_linestrings);
}


svg_writer::svg_writer(string filename, unsigned int pixel_per_in, coordinate_type scale, box_type bounding_box) :
    output_file(filename),
    bounding_box(bounding_box)
{
    const coordinate_type width =
        (bounding_box.max_corner().x() - bounding_box.min_corner().x()) * pixel_per_in / scale;
    const coordinate_type height =
        (bounding_box.max_corner().y() - bounding_box.min_corner().y()) * pixel_per_in / scale;

    //Some SVG readers does not behave well when viewBox is not specified
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"0 0 %1% %2%\"") % width % height);

    mapper = unique_ptr<bg::svg_mapper<point_type> >
        (new bg::svg_mapper<point_type>(output_file, width, height, svg_dimensions));
    mapper->add(bounding_box);
}

void svg_writer::add(const multi_polygon_type& geometry, double opacity, bool stroke)
{
    string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:2" : "";

    for (const polygon_type& poly : geometry)
    {
        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;
        multi_polygon_type mpoly;

        bg::intersection(poly, bounding_box, mpoly);

        mapper->map(mpoly,
            str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);" + stroke_str) %
            opacity % r % g % b));
    }
}

void svg_writer::add(const vector<polygon_type>& geometries, double opacity, int r, int g, int b)
{
    if (r < 0 || g < 0 || b < 0)
    {
        r = rand() % 256;
        g = rand() % 256;
        b = rand() % 256;
    }

    for (unsigned int i = geometries.size(); i != 0; i--)
    {
        multi_polygon_type mpoly;

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

