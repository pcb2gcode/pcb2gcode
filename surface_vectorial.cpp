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
        cerr << "\nWarning: Input geometry is self-intersecting.\n";

    scale = importer->vectorial_scale();

    //With a very small loss of precision we can reduce memory usage and processing time
    bg::simplify(*vectorial_surface_not_simplified, *vectorial_surface, scale / 10000);
    bg::envelope(*vectorial_surface, bounding_box);
}

vector<shared_ptr<icoords> > Surface_vectorial::get_toolpath(shared_ptr<RoutingMill> mill,
        bool mirror)
{
    coordinate_type tolerance = mill->tolerance * scale;
    // This is by how much we will grow each trace if extra passes are needed.
    coordinate_type grow = mill->tool_diameter / 2 * scale;
    shared_ptr<Isolator> isolator = dynamic_pointer_cast<Isolator>(mill);
    // Extra passes are done on each trace if requested, each offset by half the tool diameter.
    const int extra_passes = isolator ? isolator->extra_passes : 0;
    if (tolerance <= 0)
        tolerance = 0.0001 * scale;

    bg::unique(*vectorial_surface);

    box_type svg_bounding_box;

    // Make the svg file large enough to contains the width of all milling.  If it's too large, that's okay, too.
    if (grow > 0)
        bg::buffer(bounding_box, svg_bounding_box, grow * (extra_passes + 1));
    else
        bg::assign(svg_bounding_box, bounding_box);
    const string traced_filename = (boost::format("outp%d_traced_%s.svg") % debug_image_index++ % name).str();
    svg_writer debug_image(build_filename(outputdir, "processed_" + name + ".svg"), SVG_PIX_PER_IN, scale, svg_bounding_box);
    svg_writer traced_debug_image(build_filename(outputdir, traced_filename), SVG_PIX_PER_IN, scale, svg_bounding_box);

    // Get the voronoi outlines of each equipotential net.  Milling
    // should not go outside the region.  The rings are in the same order as the input
    multi_polygon_type current_mask = get_mask();
    multi_ring_type_fp voronoi_rings =
        Voronoi::get_voronoi_rings(*vectorial_surface, bounding_box, tolerance);
    vector<multi_polygon_type> voronoi_cells;
    for (const auto& ring : voronoi_rings) {
        multi_polygon_type integral_ring;
        bg::convert(ring, integral_ring);
        multi_polygon_type cell;
        bg::intersection(integral_ring, current_mask, cell);
        voronoi_cells.push_back(cell);
    }

    bool contentions = false;

    vector<shared_ptr<icoords>> toolpath;
    const auto& keep_in = voronoi_cells;
    vector<multi_polygon_type> keep_out;
    for (const auto& input : *vectorial_surface) {
        keep_out.push_back(buffer(input, grow));
    }
    bool voronoi = isolator && isolator->voronoi;
    // source_poly_index is the source of this poly in the vectorial_surface.
    auto copy_mp_to_toolpath = [&](const multi_polygon_type& mp, int source_poly_index) {
        // The color is based on the poly it surrounds.
        double which_color = (double) source_poly_index / vectorial_surface->size();
        // The path is clipped to be inside the voronoi cell and
        // outside the grown source poly.
        multi_polygon_type clipped1;
        bg::union_(mp, keep_out[source_poly_index], clipped1);
        multi_polygon_type clipped2;
        bg::intersection(clipped1, keep_in[source_poly_index], clipped2);
        debug_image.add(clipped2, 0.7/(extra_passes+1), which_color);
        traced_debug_image.add(clipped2, 1, which_color);
        multi_linestring_type mls;
        multi_poly_to_multi_linestring(clipped2, &mls);
        for (const auto& ls : mls) {
            toolpath.push_back(make_shared<icoords>());
            for (const auto& point : ls) {
                if (mirror) {
                    const coordinate_type mirror_axis = mill->mirror_absolute ?
                        bounding_box.min_corner().x() :
                        ((bounding_box.min_corner().x() + bounding_box.max_corner().x()) / 2);
                    toolpath.back()->push_back(make_pair((2 * mirror_axis - point.x()) / double(scale),
                                                         point.y() / double(scale)));
                } else {
                    toolpath.back()->push_back(make_pair(point.x() / double(scale),
                                                         point.y() / double(scale)));
                }
            }
        }
    };

    for (unsigned int i = 0; i < (*vectorial_surface).size(); i++) {
        debug_image.add((*vectorial_surface)[i], 1, (double)i/vectorial_surface->size());
    }
    const auto pass_offsets = get_pass_offsets(grow, extra_passes + 1, voronoi);
    if (voronoi) {
        // Voronoi means that we mill the voronoi cell and possibly
        // have offset milling going inward toward the net.
        for (size_t i = 0; i < voronoi_cells.size(); i++) {
            for (auto pass_offset : pass_offsets) {
                const auto buffered_poly = buffer(voronoi_cells[i], pass_offset);
                copy_mp_to_toolpath(buffered_poly, i);
            }
            // The last pass_offset will be the most contentious
            // because it's the biggest area.  Check if it was trying
            // to overlap any input traces.
            const auto& max_milling = buffer(voronoi_cells[i], pass_offsets.back() - grow);
            multi_polygon_type contentions_poly;
            bg::difference((*vectorial_surface)[i], max_milling, contentions_poly);
            if (bg::area(contentions_poly) > 0) {
                contentions = true;
                debug_image.add(contentions_poly, 1.0, 255, 0, 0);
            }
        }
    } else { // Not voronoi.
        for (size_t i = 0; i < vectorial_surface->size(); i++) {
            for (auto pass_offset : pass_offsets) {
                const auto& poly = (*vectorial_surface)[i];
                const auto buffered_poly = buffer(poly, pass_offset);
                copy_mp_to_toolpath(buffered_poly, i);
            }
            // The last pass_offset will be the most contentious
            // because it's the biggest area.  Check if it was trying
            // to overlap any input traces.
            const auto& max_milling = buffer((*vectorial_surface)[i], pass_offsets.back() + grow);
            for (size_t j = 0; j < vectorial_surface->size(); j++) {
                if (i != j) {
                    // Check if the milling overlaps the other trace.
                    multi_polygon_type contentions_poly;
                    bg::intersection((*vectorial_surface)[j], max_milling, contentions_poly);
                    if (bg::area(contentions_poly) > 0) {
                        contentions = true;
                        debug_image.add(contentions_poly, 1.0, 255, 0, 0);
                    }
                }
            }
        }
    }

    if (contentions) {
        cerr << "\nWarning: pcb2gcode hasn't been able to fulfill all"
             << " clearance requirements."
             << " You may want to check processed_" + name + ".svg, the g-code output, and"
             << " possibly use a smaller milling width.\n";
    }

    if (mill->eulerian_paths) {
        toolpath = eulerian_paths(toolpath);
    }

    tsp_solver::nearest_neighbour( toolpath, std::make_pair(0, 0), 0.0001 );

    if (mill->optimise)
    {
        vector<shared_ptr<icoords> > toolpath_optimised;
        for (const shared_ptr<icoords>& ring : toolpath)
        {
            toolpath_optimised.push_back(make_shared<icoords>());
            bg::simplify(*ring, *(toolpath_optimised.back()), mill->tolerance);
        }

        return toolpath_optimised;
    }
    else
        return toolpath;
}

void Surface_vectorial::save_debug_image(string message)
{
    const string filename = (boost::format("outp%d_%s.svg") % debug_image_index % message).str();
    svg_writer debug_image(build_filename(outputdir, filename), SVG_PIX_PER_IN, scale, bounding_box);

    for (unsigned int i = 0; i < vectorial_surface->size(); i++) {
        debug_image.add((*vectorial_surface)[i], 1, (double)i/vectorial_surface->size());
    }

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

template <typename multi_poly_t, typename multi_linestring_t>
void Surface_vectorial::multi_poly_to_multi_linestring(const multi_poly_t& mpoly, multi_linestring_t* mls) {
    // Add all the linestrings from the mpoly to the mls.
    for (const auto& poly : mpoly) {
        poly_to_multi_linestring(poly, mls);
    }
}

template <typename poly_t, typename multi_linestring_t>
void Surface_vectorial::poly_to_multi_linestring(const poly_t& poly, multi_linestring_t* mls) {
    // Add all the linestrings from the poly to the mls.
    typename multi_linestring_t::value_type ls;
    for (const auto& point : poly.outer()) {
        typename multi_linestring_t::value_type::value_type new_point(point.x(), point.y());
        ls.push_back(new_point);
    }
    mls->push_back(ls);
    for (const auto& inner : poly.inners()) {
        typename multi_linestring_t::value_type ls;
        for (const auto& point : inner) {
            typename multi_linestring_t::value_type::value_type new_point(point.x(), point.y());
            ls.push_back(new_point);
        }
        mls->push_back(ls);
    }
}

multi_polygon_type Surface_vectorial::get_mask() {
    multi_polygon_type current_mask;
    if (mask) {
        current_mask = *(mask->vectorial_surface);
    } else {
        // If there's no mask, we'll use the convex hull as a mask.
        polygon_type_fp current_mask_fp;
        multi_linestring_type_fp mls;
        multi_poly_to_multi_linestring(*vectorial_surface, &mls);
        bg::convex_hull(mls, current_mask_fp);
        bg::convert(current_mask_fp, current_mask);
    }
    return current_mask;
}

vector<shared_ptr<icoords>> Surface_vectorial::eulerian_paths(const vector<shared_ptr<icoords>>& toolpaths) {
    // First we need to split all paths so that they don't cross.
    vector<segment_type_p> all_segments;
    for (const auto& toolpath : toolpaths) {
        for (size_t i = 1; i < toolpath->size(); i++) {
            all_segments.push_back(
                segment_type_p(
                    point_type_p((*toolpath)[i-1].first*double(scale), (*toolpath)[i-1].second*double(scale)),
                    point_type_p((*toolpath)[i  ].first*double(scale), (*toolpath)[i  ].second*double(scale))));
        }
    }
    vector<segment_type_p> split_segments = segmentize(all_segments);

    multi_linestring_type segments_as_linestrings;
    for (const auto& segment : split_segments) {
        // Make a little 1-edge linestrings, filter out those that
        // aren't in the mask.
        linestring_type ls{
            point_type(segment.low().x(), segment.low().y()),
            point_type(segment.high().x(), segment.high().y())};
        segments_as_linestrings.push_back(ls);
    }

    // Make a minimal number of paths from those segments.
    struct PointLessThan {
      bool operator()(const point_type& a, const point_type& b) const {
          return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
      }
    };

    const auto& paths = get_eulerian_paths<point_type,
                                           linestring_type,
                                           multi_linestring_type,
                                           PointLessThan>(segments_as_linestrings);

    vector<shared_ptr<icoords>> new_paths;
    for (const auto& path : paths) {
        shared_ptr<icoords> new_path = make_shared<icoords>();
        for (const auto& point : path) {
            new_path->push_back(icoordpair(point.x()/double(scale), point.y()/double(scale)));
        }
        new_paths.push_back(new_path);
    }
    return new_paths;
}

vector<coordinate_type> Surface_vectorial::get_pass_offsets(coordinate_type offset, unsigned int total_passes, bool voronoi) {
    if (offset == 0) {
        return vector<coordinate_type>(total_passes, offset);
    } else if (voronoi) {
        if (total_passes % 2 == 1) {
            // It's odd, we need a center pass and then a bunch of
            // offset passes.
            vector<coordinate_type> result{0};
            total_passes--;
            for (unsigned int i = 0; i < total_passes/2; i++) {
                result.push_back(-offset * (i+1));
            }
            return result;
        } else {
            vector<coordinate_type> result;
            // It's even, so we don't have a center pass.
            for (unsigned int i = 0; i < total_passes/2; i++) {
                result.push_back(-offset/2 + -offset * i);
            }
            return result;
        }
    } else {
        // Not voronoi.
        vector<coordinate_type> result;
        for (unsigned int i = 0; i < total_passes; i++) {
            result.push_back((i+1)*offset);
        }
        return result;
    }
}

template <typename geo_t>
const multi_polygon_type Surface_vectorial::buffer(const geo_t& poly, coordinate_type offset) {
    multi_polygon_type buffered_poly;
    if (offset == 0) {
        bg::convert(poly, buffered_poly);
        return buffered_poly;
    }
    bg::buffer(poly, buffered_poly,
               bg::strategy::buffer::distance_symmetric<coordinate_type>(offset),
               bg::strategy::buffer::side_straight(),
               bg::strategy::buffer::join_round(points_per_circle),
               bg::strategy::buffer::end_round(),
               bg::strategy::buffer::point_circle(points_per_circle));
    return buffered_poly;
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

template <typename multi_geo_t>
void svg_writer::add(const multi_geo_t& geos, double opacity, unsigned int r, unsigned int g, unsigned int b) {
    for (const auto& geo : geos) {
        add(geo, opacity, r, g, b);
    }
}

template <typename multi_geo_t>
void svg_writer::add(const multi_geo_t& geos, double opacity, double which_color) {
    for (const auto& geo : geos) {
        add(geo, opacity, which_color);
    }
}

void svg_writer::add(const linestring_type& geometry, double opacity, double which_color)
{
    multi_linestring_type mlinestring;
    bg::intersection(geometry, bounding_box, mlinestring);

    unsigned int r;
    unsigned int g;
    unsigned int b;
    get_color(which_color, &r, &g, &b);
    string stroke_str = str(boost::format("stroke:rgb(%u,%u,%u);stroke-width:2") % r % g % b);
    mapper->map(mlinestring,
                str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);" + stroke_str) %
                    opacity % r % g % b));
}

void svg_writer::add(const polygon_type& poly, double opacity, double which_color)
{
    unsigned int r;
    unsigned int g;
    unsigned int b;
    get_color(which_color, &r, &g, &b);

    mapper->map(poly,
                str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);stroke:rgb(0,0,0);stroke-width:2") %
                    opacity % r % g % b));
}

void svg_writer::add(const polygon_type& poly, double opacity, unsigned int r, unsigned int g, unsigned int b)
{
    mapper->map(poly,
                str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);stroke:rgb(0,0,0);stroke-width:2") %
                    opacity % r % g % b));
}

// From https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
void svg_writer::get_color(double which_color, unsigned int *red, unsigned int *green, unsigned int *blue) {
    srand((int) (which_color*INT_MAX));

    double hh = rand() % 360;
    double s = 0.6;
    double v = 0.6;

    hh /= 60;
    int i = (int)hh;
    double ff = hh - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - (s * ff));
    double t = v * (1.0 - (s * (1.0 -ff)));
    double r, g, b;
    switch(i) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: default: r = v; g = p; b = q; break;
    }
    *red = (unsigned int)(r * 255 + 0.5);
    *green = (unsigned int)(g * 255 + 0.5);
    *blue = (unsigned int)(b * 255 + 0.5);
}
