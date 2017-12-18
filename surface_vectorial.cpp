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

    bool contentions = false;

    vector<shared_ptr<icoords>> toolpath;
    // source_poly_index is the vectorial_surface index against which
    // we won't check for contentions (because it was the source of
    // the toolpath and will overlap on the border).  Set it to -1 to
    // not skip any checks.
    auto copy_mls_to_toolpath = [&](const multi_linestring_type& mls, int source_poly_index) {
        const coordinate_type mirror_axis = mill->mirror_absolute ?
            bounding_box.min_corner().x() :
            ((bounding_box.min_corner().x() + bounding_box.max_corner().x()) / 2);
        for (unsigned int i = 0; i < mls.size(); i++) {
            double which_color;
            if (source_poly_index >= 0) {
                // The color is based on the poly it surrounds.
                which_color = (double) source_poly_index / vectorial_surface->size();
            } else {
                // Sequential color.
                which_color = (double) i / mls.size();
            }
            const auto& ls = mls[i];
            debug_image.add(ls, 0.6, which_color);
            traced_debug_image.add(ls, 1, which_color);
            multi_polygon_type milling_poly = buffer(ls, grow);
            for (int j = 0; j < (signed int) vectorial_surface->size(); j++) {
                if (j != source_poly_index) {
                    if (bg::intersects(milling_poly, (*vectorial_surface)[j])) {
                        contentions = true;
                        debug_image.add(milling_poly, 1, 0.0);
                    }
                }
            }
        }
        for (const auto& ls : mls) {
            toolpath.push_back(make_shared<icoords>());
            for (const auto& point : ls) {
                if (mirror) {
                    toolpath.back()->push_back(make_pair((2 * mirror_axis - point.x()) / double(scale),
                                                         point.y() / double(scale)));
                } else {
                    toolpath.back()->push_back(make_pair(point.x() / double(scale),
                                                         point.y() / double(scale)));
                }
            }
        }
    };

    multi_linestring_type all_linestrings;
    bool voronoi = isolator && isolator->voronoi;
    if (voronoi) {
        // First get all the segments from the current mask.
        multi_polygon_type current_mask = get_mask();
        vector<segment_type_p> all_segments;
        add_as_segments(current_mask, &all_segments);

        multi_linestring_type_fp voronoi_edges =
            Voronoi::get_voronoi_edges(*vectorial_surface, bounding_box, tolerance);
        //Add the voronoi edges to all_segments
        add_as_segments(voronoi_edges, &all_segments);

        // Split all segments where they cross and remove duplicates.
        all_segments = segmentize(all_segments);

        // Now find eulerian paths in all those segments.
        all_linestrings = eulerian_paths(all_segments, current_mask);
        for (auto pass_offset : get_pass_offsets(grow, extra_passes + 1, voronoi)) {
            if (pass_offset == 0) {
                copy_mls_to_toolpath(all_linestrings, -1);
            } else {
                multi_polygon_type buffered_poly = buffer(all_linestrings, pass_offset);
                multi_linestring_type mls;
                multi_poly_to_multi_linestring(buffered_poly, &mls);
                copy_mls_to_toolpath(mls, -1);
            }
        }
    } else {
        for (auto pass_offset : get_pass_offsets(grow, extra_passes + 1, voronoi)) {
            // We need to do each line individually because they might
            // butt up against one another during the bg::buffer and give bad paths.
            srand(1);
            for (unsigned int i = 0; i < vectorial_surface->size(); i++) {
                const auto& poly = (*vectorial_surface)[i];
                if (pass_offset == 0) {
                    multi_linestring_type mls;
                    poly_to_multi_linestring(poly, &mls);
                    copy_mls_to_toolpath(mls, i);
                } else {
                    multi_polygon_type buffered_poly = buffer(poly, pass_offset);
                    multi_linestring_type mls;
                    multi_poly_to_multi_linestring(buffered_poly, &mls);
                    copy_mls_to_toolpath(mls, i);
                }
            }
        }
    }

    for (unsigned int i = 0; i < (*vectorial_surface).size(); i++) {
        debug_image.add((*vectorial_surface)[i], 0.4, (double)i/vectorial_surface->size());
    }

    if (contentions)
    {
        cerr << "\nWarning: pcb2gcode hasn't been able to fulfill all"
             << " clearance requirements."
             << " You may want to check processed_" + name + ".svg, the g-code output, and"
             << " possibly use a smaller milling width.\n";
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

void Surface_vectorial::add_as_segments(const multi_polygon_type& mp, vector<segment_type_p> *segments) {
    for (const auto& mask_poly : mp) {
        for (size_t i = 1; i < mask_poly.outer().size(); i++) {
            segments->push_back(segment_type_p(point_type_p(mask_poly.outer()[i-1].x(), mask_poly.outer()[i-1].y()),
                                               point_type_p(mask_poly.outer()[i  ].x(), mask_poly.outer()[i  ].y())));
        }
        add_as_segments(mask_poly.inners(), segments);
    }
}

template <typename multi_linestring_t>
void Surface_vectorial::add_as_segments(const multi_linestring_t& mls, vector<segment_type_p> *segments) {
    for (const auto& ls : mls) {
        for (size_t i = 1; i < ls.size(); i++) {
            segments->push_back(segment_type_p(point_type_p(ls[i-1].x(), ls[i-1].y()),
                                               point_type_p(ls[i  ].x(), ls[i  ].y())));
        }
    }
}

multi_linestring_type Surface_vectorial::eulerian_paths(const vector<segment_type_p>& segments,
                                                        const multi_polygon_type& mask) {
    multi_linestring_type segments_as_linestrings;
    for (const auto& segment : segments) {
        // Make a little 1-edge linestrings, filter out those that
        // aren't in the mask.
        linestring_type ls{
            point_type(segment.low().x(), segment.low().y()),
                point_type(segment.high().x(), segment.high().y())};
        if (bg::covered_by(ls, mask)) {
            segments_as_linestrings.push_back(ls);
        }
    }
    // make a minimal number of paths
    struct PointLessThan {
      bool operator()(const point_type& a, const point_type& b) const {
          return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
      }
    };

    return get_eulerian_paths<point_type, linestring_type, multi_linestring_type, PointLessThan>(segments_as_linestrings);
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
                result.push_back(offset * (i+1));
            }
            return result;
        } else {
            vector<coordinate_type> result;
            // It's even, so we don't have a center pass.
            for (unsigned int i = 0; i < total_passes/2; i++) {
                result.push_back(offset/2 + offset * i);
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
multi_polygon_type Surface_vectorial::buffer(geo_t geo, coordinate_type offset) {
    multi_polygon_type buffered_poly;
    bg::buffer(geo, buffered_poly,
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

void svg_writer::get_color(double which_color, unsigned int *red, unsigned int *green, unsigned int *blue) {
    srand((int) (which_color*INT_MAX));

    double hue = rand() % 360;
    if (hue < 0) {
        hue = 0;
    } else if (hue > 360) {
        hue = 360;
    }
    if (hue == 360) hue = 0;
    hue /= 60;
    int i = (int)hue;
    double f = hue - i;
    double q = 1 - f;
    double t = f;
    double r, g, b;
    switch(i) {
    case 0: r = 1; g = t; b = 0; break;
    case 1: r = q; g = 1; b = 0; break;
    case 2: r = 0; g = 1; b = t; break;
    case 3: r = 0; g = q; b = 1; break;
    case 4: r = t; g = 0; b = 1; break;
    case 5: default: r = 1; g = 0; b = q; break;
    }
    *red = (unsigned int)(r * 255 + 0.5);
    *green = (unsigned int)(g * 255 + 0.5);
    *blue = (unsigned int)(b * 255 + 0.5);
}
