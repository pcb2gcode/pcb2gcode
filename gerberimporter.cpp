/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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

#include <algorithm>
#include <utility>
using std::reverse;
using std::copy;
using std::swap;

#include <cstdint>
#include <list>
#include <iterator>
using std::list;
using std::next;
using std::make_move_iterator;

#include <forward_list>
using std::forward_list;

#include <boost/format.hpp>

#include "gerberimporter.hpp"
#include "bg_helpers.hpp"

namespace bg = boost::geometry;

typedef bg::strategy::transform::rotate_transformer<bg::degree, double, 2, 2> rotate_deg;
typedef bg::strategy::transform::translate_transformer<coordinate_type, 2, 2> translate;

//As suggested by the Gerber specification, we retain 6 decimals
const unsigned int GerberImporter::scale = 1000000;

GerberImporter::GerberImporter(const string path) {
  project = gerbv_create_project();

  const char* cfilename = path.c_str();
  char *filename = new char[strlen(cfilename) + 1];
  strcpy(filename, cfilename);

  gerbv_open_layer_from_filename(project, filename);
  delete[] filename;

  if (project->file[0] == NULL)
    throw gerber_exception();
}

gdouble GerberImporter::get_width() {
  if (!project || !project->file[0])
    throw gerber_exception();

  return project->file[0]->image->info->max_x - project->file[0]->image->info->min_x;
}

gdouble GerberImporter::get_height() {
  if (!project || !project->file[0])
    throw gerber_exception();

  return project->file[0]->image->info->max_y - project->file[0]->image->info->min_y;
}

gdouble GerberImporter::get_min_x() {
  if (!project || !project->file[0])
    throw gerber_exception();

  return project->file[0]->image->info->min_x;
}

gdouble GerberImporter::get_max_x() {
  if (!project || !project->file[0])
    throw gerber_exception();

  return project->file[0]->image->info->max_x;
}

gdouble GerberImporter::get_min_y() {
  if (!project || !project->file[0])
    throw gerber_exception();

  return project->file[0]->image->info->min_y;
}

gdouble GerberImporter::get_max_y() {
  if (!project || !project->file[0])
    throw gerber_exception();

  return project->file[0]->image->info->max_y;
}

void GerberImporter::render(Cairo::RefPtr<Cairo::ImageSurface> surface, const guint dpi, const double min_x, const double min_y) {
  gerbv_render_info_t render_info;

  render_info.scaleFactorX = dpi;
  render_info.scaleFactorY = dpi;
  render_info.lowerLeftX = min_x;
  render_info.lowerLeftY = min_y;
  render_info.displayWidth = surface->get_width();
  render_info.displayHeight = surface->get_height();
  render_info.renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL;

  GdkColor color_saturated_white = { 0xFFFFFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
  project->file[0]->color = color_saturated_white;

  cairo_t* cr = cairo_create(surface->cobj());
  gerbv_render_layer_to_cairo_target(cr, project->file[0], &render_info);

  cairo_destroy(cr);

  /// @todo check wheter importing was successful
}

void GerberImporter::rings_to_polygons(const vector<ring_type>& rings, multi_polygon_type& mpoly) {
    list<const ring_type *> rings_ptr;
    map<const ring_type *, coordinate_type> areas;

    auto compare_areas = [&](const ring_type *a, const ring_type *b) { return areas.at(a) < areas.at(b); };

    for (const ring_type& ring : rings)
    {
        rings_ptr.push_back(&ring);
        areas[&ring] = coordinate_type(bg::area(ring));
    }

    while (!rings_ptr.empty())
    {
        mpoly.push_back(polygon_type());

        forward_list<list<const ring_type *>::iterator> to_be_removed_rings;
        auto biggest_ring = max_element(rings_ptr.begin(), rings_ptr.end(), compare_areas);
        polygon_type& current_ring = mpoly.back();

        bg::assign(current_ring.outer(), **biggest_ring);
        rings_ptr.erase(biggest_ring);

        for (auto i = rings_ptr.begin(); i != rings_ptr.end(); i++)
        {
            if (bg::within(**i, current_ring.outer()))
            {
                list<const ring_type *>::iterator j;

                for (j = rings_ptr.begin(); j != rings_ptr.end(); j++)
                    if (i != j && bg::within(**i, **j))
                            break;

                if (j == rings_ptr.end())
                {
                    current_ring.inners().push_back(ring_type());
                    current_ring.inners().back().reserve((*i)->size());

                    for (auto point = (*i)->rbegin(); point != (*i)->rend(); point++)
                    {
                        current_ring.inners().back().push_back(*point);
                    }
                    to_be_removed_rings.push_front(i);
                }
            }
        }

        for (auto i : to_be_removed_rings)
            rings_ptr.erase(i);
    }
}

// Draw a regular polygon with outer diameter as specified and center.  The
// number of vertices is provided.  offset is an angle in degrees to the
// starting vertex of the shape.  clockwise to put the vertices in clockwise
// order.
ring_type make_regular_polygon(point_type center, coordinate_type diameter, unsigned int vertices,
                               double offset, bool clockwise) {
  double angle_step;

  if (clockwise)
    angle_step = -2 * bg::math::pi<double>() / vertices;
  else
    angle_step = 2 * bg::math::pi<double>() / vertices;

  offset *= bg::math::pi<double>() / 180.0;

  ring_type ring;
  for (unsigned int i = 0; i < vertices; i++)
    ring.push_back(point_type(cos(angle_step * i + offset) * diameter / 2 + center.x(),
                              sin(angle_step * i + offset) * diameter / 2 + center.y()));

  ring.push_back(ring.front()); // Don't forget to close the ring.
  return ring;
}

polygon_type make_regular_polygon(point_type center, coordinate_type diameter, unsigned int vertices,
                               coordinate_type offset, coordinate_type hole_diameter,
                               unsigned int circle_points) {
  polygon_type polygon;
  polygon.outer() = make_regular_polygon(center, diameter, vertices, offset, true);

  if (hole_diameter > 0) {
    polygon.inners().push_back(make_regular_polygon(center, hole_diameter, circle_points, 0, false));
  }
  return polygon;
}

polygon_type make_rectangle(point_type center, double width, double height,
                            coordinate_type hole_diameter, unsigned int circle_points) {
  polygon_type polygon;
  const coordinate_type x = center.x();
  const coordinate_type y = center.y();

  polygon.outer().push_back(point_type(x - width / 2, y - height / 2));
  polygon.outer().push_back(point_type(x - width / 2, y + height / 2));
  polygon.outer().push_back(point_type(x + width / 2, y + height / 2));
  polygon.outer().push_back(point_type(x + width / 2, y - height / 2));
  polygon.outer().push_back(polygon.outer().front());

  if (hole_diameter != 0) {
    polygon.inners().push_back(make_regular_polygon(center, hole_diameter, circle_points, 0, false));
  }
  return polygon;
}

polygon_type make_rectangle(point_type point1, point_type point2, double height) {
  polygon_type polygon;
  const double distance = bg::distance(point1, point2);
  const double normalized_dy = (point2.y() - point1.y()) / distance;
  const double normalized_dx = (point2.x() - point1.x()) / distance;
  // Rotate the normalized vectors by 90 degrees ccw.
  const double dy = height / 2 * normalized_dx;
  const double dx = height / 2 * -normalized_dy;

  polygon.outer().push_back(point_type(point1.x() - dx, point1.y() - dy));
  polygon.outer().push_back(point_type(point1.x() + dx, point1.y() + dy));
  polygon.outer().push_back(point_type(point2.x() + dx, point2.y() + dy));
  polygon.outer().push_back(point_type(point2.x() - dx, point2.y() - dy));
  polygon.outer().push_back(polygon.outer().front());

  return polygon;
}

multi_polygon_type make_oval(point_type center, coordinate_type width, coordinate_type height,
                             coordinate_type hole_diameter, unsigned int circle_points) {
  point_type_fp start(center.x(), center.y());
  point_type_fp end(center.x(), center.y());
  if (width > height) {
    // The oval is more wide than tall.
    start.x(start.x() - (width - height)/2);
    end.x(end.x() + (width - height)/2);
  } else {
    // The oval is more tall than wide.
    start.y(start.y() - (height - width)/2);
    end.y(end.y() + (height - width)/2);
  }
  // TODO: Make sure this works when width==height.

  multi_polygon_type_fp oval;
  bg::buffer(linestring_type_fp{start, end}, oval,
             bg::strategy::buffer::distance_symmetric<coordinate_type>(std::min(width, height)/2),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(circle_points),
             bg::strategy::buffer::end_round(circle_points),
             bg::strategy::buffer::point_circle(circle_points));

  if (hole_diameter > 0) {
    multi_polygon_type_fp temp;
    auto hole = make_regular_polygon(center, hole_diameter, circle_points, 0, true);
    multi_polygon_type_fp hole_fp;
    bg::convert(hole, hole_fp);
    bg::difference(oval, hole_fp, temp);
    oval = temp;
  }
  multi_polygon_type ret;
  bg::convert(oval, ret);
  return ret;
}

multi_polygon_type linear_draw_rectangular_aperture(point_type startpoint, point_type endpoint, coordinate_type width,
                                                    coordinate_type height) {
  multi_polygon_type hull_input;
  hull_input.push_back(make_rectangle(startpoint, width, height, 0, 0));
  hull_input.push_back(make_rectangle(endpoint, width, height, 0, 0));
  multi_polygon_type hull;
  hull.resize(1);
  bg::convex_hull(hull_input, hull[0]);
  return hull;
}

multi_polygon_type linear_draw_circular_aperture(point_type startpoint, point_type endpoint,
                                                 coordinate_type radius, unsigned int circle_points) {
  multi_polygon_type oval;
  bg::buffer(linestring_type{startpoint, endpoint}, oval,
             bg::strategy::buffer::distance_symmetric<coordinate_type>(radius),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(circle_points),
             bg::strategy::buffer::end_round(circle_points),
             bg::strategy::buffer::point_circle(circle_points));
  return oval;
}

void GerberImporter::circular_arc(point_type center, coordinate_type radius,
                    double angle1, double angle2, unsigned int circle_points,
                    linestring_type& linestring)
{
    const unsigned int steps = ceil((angle2 - angle1) / (2 * bg::math::pi<double>()) * circle_points);
    const double angle_step = (angle2 - angle1) / steps;
    
    for (unsigned int i = 0; i < steps; i++)
    {
        const double angle = angle1 + i * angle_step;

        linestring.push_back(point_type(cos(angle) * radius + center.x(),
                                        sin(angle) * radius + center.y()));
    }
    
    linestring.push_back(point_type(cos(angle2) * radius + center.x(),
                                    sin(angle2) * radius + center.y()));
}
void GerberImporter::merge_paths(multi_linestring_type &destination, const linestring_type& source, double tolerance)
{
    const double comparable_tolerance = tolerance * tolerance;

    if (!destination.empty())
    {
        auto check_connection = [&](const point_type &point, multi_linestring_type::reverse_iterator ls,
                                    connected_linestring &conn_ls)
        {
            double comparable_distance = bg::comparable_distance(point, ls->back());
            if (comparable_distance <= comparable_tolerance)
            {
                conn_ls.ls = ls;
                conn_ls.side = BACK;
                conn_ls.approximated = !(comparable_distance == 0);
            }
            else
            {
                comparable_distance = bg::comparable_distance(point, ls->front());
                if (comparable_distance <= comparable_tolerance)
                {
                    conn_ls.ls = ls;
                    conn_ls.side = FRONT;
                    conn_ls.approximated = !(comparable_distance == 0);
                }
            }
        };

        auto merge_linestring = [](const linestring_type& source, Side source_side,
                                   linestring_type& destination, Side destination_side, bool approximated)
        {
            if (destination_side == FRONT)
                reverse(destination.begin(), destination.end());

            if (source_side == FRONT)
            {
                if (approximated)
                    destination.insert(destination.end(), source.begin(), source.end());
                else
                    destination.insert(destination.end(), source.begin() + 1, source.end());
            }
            else
            {
                if (approximated)
                    destination.insert(destination.end(), source.rbegin(), source.rend());
                else
                    destination.insert(destination.end(), source.rbegin() + 1, source.rend());
            }
        };

        auto close_if_necessary = [&](linestring_type& ls)
        {
            if (!bg::equals(ls.front(), ls.back()) &&
                bg::comparable_distance(ls.front(), ls.back()) <= comparable_tolerance)
                ls.push_back(ls.front());
        };

        const point_type *source_sides[2] = {&source.front(), &source.back()};
        connected_linestring conn_lss[2];
        multi_linestring_type::reverse_iterator ls;

        conn_lss[FRONT].ls = destination.rend();
        conn_lss[BACK].ls = destination.rend();

        for (unsigned int i = 0; i < 2; i++)
        {
            for (ls = destination.rbegin(); ls != destination.rend() && conn_lss[i].ls == destination.rend(); ls++)
            {
                check_connection(*source_sides[i], ls, conn_lss[i]);
            }
        }

        int case_num = (int(conn_lss[BACK].ls != destination.rend()) << 1) +
                        int(conn_lss[FRONT].ls != destination.rend());

        if (case_num == 3 && conn_lss[FRONT].ls == conn_lss[BACK].ls)
        {
            if (conn_lss[FRONT].side == BACK)
                case_num = 1;
            else
                case_num = 2;
        }

        switch (case_num)
        {
        case 0: //No junctions
            destination.push_back(source);
            break;

        case 1: //Front can be connected
            merge_linestring(source, FRONT, *conn_lss[FRONT].ls, conn_lss[FRONT].side, conn_lss[FRONT].approximated);
            close_if_necessary(*conn_lss[FRONT].ls);
            break;

        case 2: //Back can be connected
            merge_linestring(source, BACK, *conn_lss[BACK].ls, conn_lss[BACK].side, conn_lss[BACK].approximated);
            close_if_necessary(*conn_lss[BACK].ls);
            break;

        case 3: //Both front and back can be connected
            merge_linestring(source, FRONT, *conn_lss[FRONT].ls, conn_lss[FRONT].side, conn_lss[FRONT].approximated);
            merge_linestring(*conn_lss[BACK].ls, conn_lss[BACK].side, *conn_lss[FRONT].ls, BACK, conn_lss[BACK].approximated);
            destination.erase(prev(conn_lss[BACK].ls.base()));
            break;
        }
    }
    else
    {
        destination.push_back(source);
    }
}

void GerberImporter::simplify_paths(multi_linestring_type &paths)
{
    for (auto path1 = paths.begin(); path1 != paths.end(); path1++)
    {
        if (!path1->empty())
        {
            for (auto path2 = paths.begin(); path2 != paths.end(); path2++)
            {
                if (!path2->empty() && path1 != path2)
                {
                    if (bg::equals(path1->back(), path2->front()))
                    {
                        path1->insert(path1->end(),
                                      make_move_iterator(next(path2->begin())),
                                      make_move_iterator(path2->end()));

                        path2->clear();
                    }
                    else if (bg::equals(path1->back(), path2->back()))
                    {
                        if (path1->size() < path2->size())
                        {
                            path2->insert(path2->end(),
                                          make_move_iterator(next(path1->rbegin())),
                                          make_move_iterator(path1->rend()));
                            path1->swap(*path2);
                        }
                        else
                            path1->insert(path1->end(),
                                          make_move_iterator(next(path2->rbegin())),
                                          make_move_iterator(path2->rend()));

                        path2->clear();
                    }
                    else if (bg::equals(path1->front(), path2->back()))
                    {
                        path2->insert(path2->end(),
                                      make_move_iterator(next(path1->begin())),
                                      make_move_iterator(path1->end()));
                        path1->swap(*path2);

                        path2->clear();
                    }
                    else if (bg::equals(path1->front(), path2->front()))
                    {
                        std::reverse(path1->begin(), path1->end());
                        path1->insert(path1->end(),
                                      make_move_iterator(next(path2->begin())),
                                      make_move_iterator(path2->end()));

                        path2->clear();
                    }
                }
            }
        }
    }

    auto isEmpty = [&](const linestring_type& ls)
    {
        return ls.empty();
    };

    paths.erase(std::remove_if(paths.begin(), paths.end(), isEmpty), paths.end());
}

unique_ptr<multi_polygon_type> GerberImporter::generate_layers(vector<pair<const gerbv_layer_t *, gerberimporter_layer> >& layers,
                                                                bool fill_rings, coordinate_type cfactor, unsigned int points_per_circle)
{
    unique_ptr<multi_polygon_type> output (new multi_polygon_type());
    vector<ring_type> rings;

    for (auto layer = layers.begin(); layer != layers.end(); layer++)
    {
        unique_ptr<multi_polygon_type> temp_mpoly (new multi_polygon_type());
        const gerbv_polarity_t polarity = layer->first->polarity;
        const gerbv_step_and_repeat_t& stepAndRepeat = layer->first->stepAndRepeat;
        map<coordinate_type, multi_linestring_type>& paths = layer->second.paths;
        unique_ptr<multi_polygon_type>& draws = layer->second.draws;

        const unsigned int layer_rings_offset = rings.size();

        for (auto i = paths.begin(); i != paths.end(); i++)
        {
            if (fill_rings)
            {
                for (auto ls = i->second.begin(); ls != i->second.end(); )
                {
                    if (ls->size() >= 4)
                    {
                        multi_point_type intersection_points;
                        segment_type first_segment(ls->front(), ls->at(1));
                        segment_type last_segment(ls->back(), ls->at(ls->size() - 2));

                        bg::intersection(first_segment, last_segment, intersection_points);

                        if (!intersection_points.empty())
                        {
                            bg::assign(ls->front(), intersection_points.front());
                            bg::assign(ls->back(), intersection_points.front());
                        }
                    }

                    if (bg::equals(ls->front(), ls->back()) && bg::is_valid(*ls))
                    {
                        rings.push_back(ring_type());
                        rings.back().reserve(ls->size());

                        for (auto point = ls->begin(); point != ls->end(); point++)
                        {
                            rings.back().push_back(*point);
                        }
                        bg::correct(rings.back());

                        ls = i->second.erase(ls);
                    }
                    else
                        ++ls;
                }
            }

            if (i->second.size() != 0)
            {
                // Always convert to floating point before calling
                // bg::buffer because it is buggy with fixed point.
                multi_polygon_type buffered_mls;
                bg_helpers::buffer(i->second, buffered_mls, i->first);
                bg::union_(buffered_mls, *draws, *temp_mpoly);
                temp_mpoly.swap(draws);
                temp_mpoly->clear();
            }
        }
        
        paths.clear();

        if (fill_rings)
        {
            unsigned int newrings = rings.size() - layer_rings_offset;
            unsigned int translated_ring_offset = rings.size();

            rings.resize(layer_rings_offset + newrings * stepAndRepeat.X * stepAndRepeat.Y);

            for (int sr_x = 0; sr_x < stepAndRepeat.X; sr_x++)
            {
                for (int sr_y = 0; sr_y < stepAndRepeat.Y; sr_y++)
                {
                    if (sr_x != 0 || sr_y != 0)
                    {
                        translate translate_strategy(stepAndRepeat.dist_X * sr_x * cfactor,
                                                     stepAndRepeat.dist_Y * sr_y * cfactor);

                        for (unsigned int i = 0; i < newrings; i++)
                        {
                            bg::transform(rings[layer_rings_offset + i], rings[translated_ring_offset], translate_strategy);
                            ++translated_ring_offset;
                        }
                    }
                }
            }
        }

        for (int sr_x = 1; sr_x < stepAndRepeat.X; sr_x++)
        {
            multi_polygon_type translated_mpoly;
            unique_ptr<multi_polygon_type> united_mpoly (new multi_polygon_type());

            bg::transform(*draws, translated_mpoly,
                            translate(stepAndRepeat.dist_X * sr_x * cfactor, 0));
            
            if (sr_x == 1)
                bg::union_(translated_mpoly, *draws, *united_mpoly);
            else
                bg::union_(translated_mpoly, *temp_mpoly, *united_mpoly);

            united_mpoly.swap(temp_mpoly);
        }

        if (stepAndRepeat.X > 1)
        {
            temp_mpoly.swap(draws);
            temp_mpoly->clear();
        }

        for (int sr_y = 1; sr_y < stepAndRepeat.Y; sr_y++)
        {
            multi_polygon_type translated_mpoly;
            unique_ptr<multi_polygon_type> united_mpoly (new multi_polygon_type());

            bg::transform(*draws, translated_mpoly,
                            translate(0, stepAndRepeat.dist_Y * sr_y * cfactor));

            if (sr_y == 1)
                bg::union_(translated_mpoly, *draws, *united_mpoly);
            else
                bg::union_(translated_mpoly, *temp_mpoly, *united_mpoly);

            united_mpoly.swap(temp_mpoly);
        }
        
        if (stepAndRepeat.Y > 1)
        {
            temp_mpoly.swap(draws);
            temp_mpoly->clear();
        }
        
        if (layer != layers.begin())
        {
            if (polarity == GERBV_POLARITY_DARK)
                bg::union_(*output, *draws, *temp_mpoly);
            else if (polarity == GERBV_POLARITY_CLEAR)
                bg::difference(*output, *draws, *temp_mpoly);
            else
                unsupported_polarity_throw_exception();

            temp_mpoly.swap(output);
        }
        else
        {
            if (polarity == GERBV_POLARITY_DARK)
                output.swap(draws);
            else if (polarity != GERBV_POLARITY_CLEAR)
                unsupported_polarity_throw_exception();
        }
        
        draws->clear();
    }

    if (fill_rings)
    {
        multi_polygon_type filled_rings;
        unique_ptr<multi_polygon_type> merged_mpoly (new multi_polygon_type());

        rings_to_polygons(rings, filled_rings);
        bg::union_(filled_rings, *output, *merged_mpoly);
        output.swap(merged_mpoly);
    }

    return output;
}

polygon_type make_moire(const double * const parameters, unsigned int circle_points,
                        coordinate_type cfactor) {
  const point_type center(parameters[0] * cfactor, parameters[1] * cfactor);
  multi_polygon_type moire_parts;

  double crosshair_thickness = parameters[6];
  double crosshair_length = parameters[7];
  moire_parts.push_back(make_rectangle(center, crosshair_thickness * cfactor, crosshair_length * cfactor, 0, 0));
  moire_parts.push_back(make_rectangle(center, crosshair_length * cfactor, crosshair_thickness * cfactor, 0, 0));
  const int max_number_of_rings = parameters[5];
  const double outer_ring_diameter = parameters[2];
  const double ring_thickness = parameters[3];
  const double gap_thickness = parameters[4];
  for (int i = 0; i < max_number_of_rings; i++) {
    const double external_diameter = outer_ring_diameter - 2 * (ring_thickness + gap_thickness) * i;
    double internal_diameter = external_diameter - 2 * ring_thickness;
    if (external_diameter <= 0)
      break;
    if (internal_diameter < 0)
      internal_diameter = 0;
    moire_parts.push_back(make_regular_polygon(center, external_diameter * cfactor, circle_points, 0,
                                               internal_diameter * cfactor, circle_points));
  }
  multi_polygon_type moire;
  for (const auto& p : moire_parts) {
    multi_polygon_type union_temp;
    bg::union_(moire, p, union_temp);
    moire = union_temp;
  }
  return moire.front();
}

void GerberImporter::draw_thermal(point_type center, coordinate_type external_diameter, coordinate_type internal_diameter,
                                    coordinate_type gap_width, unsigned int circle_points, multi_polygon_type& output)
{
    polygon_type rect1;
    polygon_type rect2;
    multi_polygon_type cross;

    polygon_type ring = make_regular_polygon(center, external_diameter, circle_points,
                                             0, internal_diameter, circle_points);

    rect1 = make_rectangle(center, gap_width, 2 * external_diameter, 0, 0);
    rect2 = make_rectangle(center, 2 * external_diameter, gap_width, 0, 0);
    bg::union_(rect1, rect2, cross);
    bg::difference(ring, cross, output);
}

// Look through ring for crossing points and snip them out of the input so that
// the return value is a series of rings such that no ring has the same point in
// it twice.
vector<ring_type> get_all_rings(const ring_type& ring) {
  for (auto start = ring.cbegin(); start != ring.cend(); start++) {
    for (auto end = std::next(start); end != ring.cend(); end++) {
      if (bg::equals(*start, *end)) {
        if (start == ring.cbegin() && end == std::prev(ring.cend())) {
          continue; // This is just the entire ring, no need to try to recurse here.
        }
        ring_type inner_ring(start, end); // Build the ring that we've found.
        inner_ring.push_back(inner_ring.front()); // Close the ring.

        // Make a ring from the rest of the points.
        ring_type outer_ring(ring.cbegin(), start);
        outer_ring.insert(outer_ring.end(), end, ring.cend());
        // Recurse on outer and inner and put together.
        vector<ring_type> all_rings = get_all_rings(outer_ring);
        vector<ring_type> all_inner_rings = get_all_rings(inner_ring);
        all_rings.insert(all_rings.end(), all_inner_rings.cbegin(), all_inner_rings.cend());
        return all_rings;
      }
    }
  }
  // No points repeated so just return the original without recursion.
  return vector<ring_type>{ring};
}

multi_polygon_type simplify_cutins(const ring_type& ring) {
  const auto area = bg::area(ring); // Positive if the original ring is clockwise, otherwise negative.
  vector<ring_type> all_rings = get_all_rings(ring);
  multi_polygon_type ret;
  for (auto r : all_rings) {
    const auto this_area = bg::area(r);
    if (r.size() < 4 || this_area == 0) {
      continue; // No area so ignore it.
    }
    if (this_area * area > 0) {
      multi_polygon_type temp_ret;
      auto correct_r = r;
      bg::correct(correct_r);
      bg::union_(ret, correct_r, temp_ret);
      ret = temp_ret;
    }
  }
  for (auto r : all_rings) {
    const auto this_area = bg::area(r);
    if (r.size() < 4 || this_area == 0) {
      continue; // No area so ignore it.
    }
    if (this_area * area < 0) {
      multi_polygon_type temp_ret;
      auto correct_r = r;
      bg::correct(correct_r);
      bg::difference(ret, correct_r, temp_ret);
      ret = temp_ret;
    }
  }
  return ret;
}

void GerberImporter::generate_apertures_map(const gerbv_aperture_t * const apertures[], map<int, multi_polygon_type>& apertures_map, unsigned int circle_points, coordinate_type cfactor)
{
    const point_type origin (0, 0);

    for (int i = 0; i < APERTURE_MAX; i++)
    {
        const gerbv_aperture_t * const aperture = apertures[i];

        if (aperture)
        {
            const double * const parameters = aperture->parameter;
            unique_ptr<multi_polygon_type> input (new multi_polygon_type());
            unique_ptr<multi_polygon_type> output (new multi_polygon_type());

            switch (aperture->type)
            {
                case GERBV_APTYPE_NONE:
                    continue;

                case GERBV_APTYPE_CIRCLE:
                  input->push_back(make_regular_polygon(origin,
                                                        parameters[0] * cfactor,
                                                        circle_points,
                                                        parameters[1] * cfactor,
                                                        parameters[2] * cfactor,
                                                        circle_points));
                  break;

                case GERBV_APTYPE_RECTANGLE:
                  input->push_back(make_rectangle(origin,
                                                  parameters[0] * cfactor,
                                                  parameters[1] * cfactor,
                                                  parameters[2] * cfactor,
                                                  circle_points));
                  break;

                case GERBV_APTYPE_OVAL:
                  *input = make_oval(origin,
                                     parameters[0] * cfactor,
                                     parameters[1] * cfactor,
                                     parameters[2] * cfactor,
                                     circle_points);
                    break;

                case GERBV_APTYPE_POLYGON:
                    input->push_back(make_regular_polygon(origin,
                                                         parameters[0] * cfactor,
                                                         parameters[1] * cfactor,
                                                         parameters[2] * cfactor,
                                                         parameters[3] * cfactor,
                                                         circle_points));
                    break;
                
                case GERBV_APTYPE_MACRO:
                    if (aperture->simplified)
                    {
                        const gerbv_simplified_amacro_t *simplified_amacro = aperture->simplified;

                        while (simplified_amacro)
                        {
                            const double * const parameters = simplified_amacro->parameter;
                            double rotation;
                            int polarity;
                            multi_polygon_type mpoly;
                            multi_polygon_type mpoly_rotated;

                            switch (simplified_amacro->type)
                            {                     
                                case GERBV_APTYPE_NONE:
	                            case GERBV_APTYPE_CIRCLE:
	                            case GERBV_APTYPE_RECTANGLE:
	                            case GERBV_APTYPE_OVAL:
	                            case GERBV_APTYPE_POLYGON:
	                                cerr << "Non-macro aperture during macro drawing: skipping" << endl;
	                                simplified_amacro = simplified_amacro->next;
	                                continue;
	
	                            case GERBV_APTYPE_MACRO:
	                                cerr << "Macro start aperture during macro drawing: skipping" << endl;
	                                simplified_amacro = simplified_amacro->next;
	                                continue;
	
	                            case GERBV_APTYPE_MACRO_CIRCLE:
	                                mpoly.resize(1);
	                                mpoly.front().outer() = make_regular_polygon(point_type(parameters[2] * cfactor, parameters[3] * cfactor),
                                                                                     parameters[1] * cfactor,
                                                                                     circle_points,
                                                                                     0, true);
	                                polarity = parameters[0];
	                                rotation = parameters[4];
	                                break;
	                            
	                            case GERBV_APTYPE_MACRO_OUTLINE:
	                                mpoly.resize(1);
                                    for (unsigned int i = 0; i < round(parameters[1]) + 1; i++)
                                    {
                                        mpoly.front().outer().push_back(point_type(parameters[i * 2 + 2] * cfactor,
                                                                                    parameters [i * 2 + 3] * cfactor));
                                    }
                                    bg::correct(mpoly.front());
                                    {
                                      auto new_polys = simplify_cutins(mpoly.front().outer());
                                      mpoly.erase(mpoly.begin());
                                      mpoly.insert(mpoly.end(), new_polys.cbegin(), new_polys.cend());
                                    }
                                    polarity = parameters[0];
                                    rotation = parameters[(2 * int(round(parameters[1])) + 4)];
                                    break;
	                            
	                            case GERBV_APTYPE_MACRO_POLYGON:
	                                mpoly.resize(1);
	                                mpoly.front().outer() = make_regular_polygon(point_type(parameters[2] * cfactor, parameters[3] * cfactor),
                                                                                     parameters[4] * cfactor,
                                                                                     parameters[1],
                                                                                     0, true);
	                                polarity = parameters[0];
	                                rotation = parameters[5];
	                                break;
	                            
	                            case GERBV_APTYPE_MACRO_MOIRE:
                                      mpoly.push_back(make_moire(parameters, circle_points, cfactor));
                                      polarity = 1;
                                      rotation = parameters[8];
                                      break;

	                            case GERBV_APTYPE_MACRO_THERMAL:
	                                draw_thermal(point_type(parameters[0] * cfactor, parameters[1] * cfactor),
	                                                parameters[2] * cfactor,
	                                                parameters[3] * cfactor,
	                                                parameters[4] * cfactor,
	                                                circle_points,
	                                                mpoly);
	                                polarity = 1;
	                                rotation = parameters[5];
	                                break;

	                            case GERBV_APTYPE_MACRO_LINE20:
                                      mpoly.push_back(make_rectangle(point_type(parameters[2] * cfactor, parameters[3] * cfactor),
                                                                     point_type(parameters[4] * cfactor, parameters[5] * cfactor),
                                                                     parameters[1] * cfactor));
                                      polarity = parameters[0];
                                      rotation = parameters[6];
                                      break;

	                            case GERBV_APTYPE_MACRO_LINE21:
                                      mpoly.push_back(make_rectangle(point_type(parameters[3] * cfactor, parameters[4] * cfactor),
                                                                     parameters[1] * cfactor,
                                                                     parameters[2] * cfactor,
                                                                     0, 0));
                                      polarity = parameters[0];
                                      rotation = parameters[5];
                                      break;

	                            case GERBV_APTYPE_MACRO_LINE22:
                                      mpoly.push_back(make_rectangle(point_type((parameters[3] + parameters[1] / 2) * cfactor,
                                                                                (parameters[4] + parameters[2] / 2) * cfactor),
                                                                     parameters[1] * cfactor,
                                                                     parameters[2] * cfactor,
                                                                     0, 0));
                                      polarity = parameters[0];
                                      rotation = parameters[5];
                                      break;

	                            default:
                                    cerr << "Unrecognized aperture: skipping" << endl;
                                    simplified_amacro = simplified_amacro->next;
                                    continue;
                            }

                            // For Boost.Geometry a positive angle is considered
                            // clockwise, for Gerber is the opposite
                            bg::transform(mpoly, mpoly_rotated, rotate_deg(-rotation));

                            if (polarity == 0)
                                bg::difference(*input, mpoly_rotated, *output);
                            else
                                bg::union_(*input, mpoly_rotated, *output);
                            input->clear();
                            input.swap(output);
                        
                            simplified_amacro = simplified_amacro->next;
                        }
                    }
                    else
                    {
                        cerr << "Macro aperture " << i << " is not simplified: skipping" << endl;
                        continue;
                    }
                    break;
                
                case GERBV_APTYPE_MACRO_CIRCLE:
	            case GERBV_APTYPE_MACRO_OUTLINE:
	            case GERBV_APTYPE_MACRO_POLYGON:
	            case GERBV_APTYPE_MACRO_MOIRE:
	            case GERBV_APTYPE_MACRO_THERMAL:
	            case GERBV_APTYPE_MACRO_LINE20:
	            case GERBV_APTYPE_MACRO_LINE21:
	            case GERBV_APTYPE_MACRO_LINE22:
	                cerr << "Macro aperture during non-macro drawing: skipping" << endl;
	                continue;
	            
	            default:
	                cerr << "Unrecognized aperture: skipping" << endl;
                    continue;
            }

            apertures_map[i] = *input;
        }
    }
}

bool layers_equivalent(const gerbv_layer_t* const layer1, const gerbv_layer_t* const layer2) {
  const gerbv_step_and_repeat_t& sr1 = layer1->stepAndRepeat;
  const gerbv_step_and_repeat_t& sr2 = layer2->stepAndRepeat;

  return (layer1->polarity == layer2->polarity &&
          sr1.X == sr2.X &&
          sr1.Y == sr2.Y &&
          sr1.dist_X == sr2.dist_X &&
          sr1.dist_Y == sr2.dist_Y);
}

// Convert the gerber file into a multi_polygon_type.  If fill_closed_lines is
// true, return all closed shapes without holes in them.  points_per_circle is
// the number of lines to use to appoximate circles.
unique_ptr<multi_polygon_type> GerberImporter::render(bool fill_closed_lines, unsigned int points_per_circle) {
  map<int, multi_polygon_type> apertures_map;
  ring_type region;
  coordinate_type cfactor;
  unique_ptr<multi_polygon_type> temp_mpoly (new multi_polygon_type());
  bool contour = false; // Are we in contour mode?

  vector<pair<const gerbv_layer_t *, gerberimporter_layer> >layers (1);

  gerbv_image_t *gerber = project->file[0]->image;

  if (gerber->info->polarity != GERBV_POLARITY_POSITIVE)
    unsupported_polarity_throw_exception();

  if (gerber->netlist->state->unit == GERBV_UNIT_MM) {
    cfactor = scale / 25.4;
  } else {
    cfactor = scale;
  }

  generate_apertures_map(gerber->aperture, apertures_map, points_per_circle, cfactor);
  layers.front().first = gerber->netlist->layer;


    for (gerbv_net_t *currentNet = gerber->netlist; currentNet; currentNet = currentNet->next){

        const point_type start (currentNet->start_x * cfactor, currentNet->start_y * cfactor);
        const point_type stop (currentNet->stop_x * cfactor, currentNet->stop_y * cfactor);
        const double * const parameters = gerber->aperture[currentNet->aperture]->parameter;
        multi_polygon_type mpoly;

        if (!layers_equivalent(currentNet->layer, layers.back().first))
        {
            layers.resize(layers.size() + 1);
            layers.back().first = currentNet->layer;
        }

        map<coordinate_type, multi_linestring_type>& paths = layers.back().second.paths;
        unique_ptr<multi_polygon_type>& draws = layers.back().second.draws;

        auto merge_ring = [&](ring_type& ring)
        {
            if (ring.size() > 1)
            {
                polygon_type polygon;
                bg::correct(ring);
                
                bg::union_(*draws, simplify_cutins(ring), *temp_mpoly);

                ring.clear();
                draws.swap(temp_mpoly);
                temp_mpoly->clear();
            }
        };

        auto merge_mpoly = [&](multi_polygon_type& mpoly)
        {
            bg::correct(mpoly);
            bg::union_(*draws, mpoly, *temp_mpoly);
            mpoly.clear();
            draws.swap(temp_mpoly);
            temp_mpoly->clear();
        };

        if (currentNet->interpolation == GERBV_INTERPOLATION_LINEARx1) {
        
            if (currentNet->aperture_state == GERBV_APERTURE_STATE_ON) {
            
                if (contour)
                {
                    if (region.empty())
                        bg::append(region, start);

                    bg::append(region, stop);
                }
                else
                {
                    if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_CIRCLE)
                    {
                        const double diameter = parameters[0] * cfactor;
                        linestring_type new_segment;

                        new_segment.push_back(start);
                        new_segment.push_back(stop);

                        merge_paths(paths[coordinate_type(diameter / 2)], new_segment, fill_closed_lines ? diameter : 0);
                    }
                    else if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_RECTANGLE) {
                      mpoly = linear_draw_rectangular_aperture(start, stop, parameters[0] * cfactor,
                                                               parameters[1] * cfactor);
                      merge_ring(mpoly.back().outer());
                    }
                    else
                        cerr << "Drawing with an aperture different from a circle "
                                     "or a rectangle is forbidden by the Gerber standard; skipping."
                                  << endl;
                }
            }
            
            else if (currentNet->aperture_state == GERBV_APERTURE_STATE_FLASH) {

                if (contour)
                {
                    cerr << "D03 during contour mode is forbidden by the Gerber "
                                    "standard; skipping" << endl;
                }
                else
                {
                    const auto aperture_mpoly = apertures_map.find(currentNet->aperture);

                    if (aperture_mpoly != apertures_map.end())
                        bg::transform(aperture_mpoly->second, mpoly, translate(stop.x(), stop.y()));
                    else
                        cerr << "Macro aperture " << currentNet->aperture <<
                                    " not found in macros list; skipping" << endl;

                    merge_mpoly(mpoly);
                }
            }
            else if (currentNet->aperture_state == GERBV_APERTURE_STATE_OFF)
            {
                if (contour)
                {
                    if (!region.empty())
                    {
                        bg::append(region, stop);
                        merge_ring(region);
                    }
                }
            }
            else
            {
                cerr << "Unrecognized aperture state: skipping" << endl;
            }
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_START)
        {
            contour = true;
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_END)
        {
            contour = false;
            
            if (!region.empty())
                merge_ring(region);
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR ||
                 currentNet->interpolation == GERBV_INTERPOLATION_CCW_CIRCULAR)
        {
            if (currentNet->aperture_state == GERBV_APERTURE_STATE_ON) {
                const gerbv_cirseg_t * const cirseg = currentNet->cirseg;
                linestring_type path;

                if (cirseg != NULL)
                {
                    double angle1;
                    double angle2;

                    if (currentNet->interpolation == GERBV_INTERPOLATION_CCW_CIRCULAR)
                    {
                        angle1 = cirseg->angle1;
                        angle2 = cirseg->angle2;
                    }
                    else
                    {
                        angle1 = cirseg->angle2;
                        angle2 = cirseg->angle1;
                    }

                    circular_arc(point_type(cirseg->cp_x * scale, cirseg->cp_y * scale),
                                    cirseg->width * scale / 2,
                                    angle1 * bg::math::pi<double>() / 180.0,
                                    angle2 * bg::math::pi<double>() / 180.0,
                                    points_per_circle,
                                    path);

                    if (contour)
                    {
                        if (region.empty())
                            copy(path.begin(), path.end(), region.end());
                        else
                            copy(path.begin() + 1, path.end(), region.end());
                    }
                    else
                    {
                        if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_CIRCLE)
                        {
                            const double diameter = parameters[0] * cfactor;
                            merge_paths(paths[coordinate_type(diameter / 2)], path, fill_closed_lines ? diameter : 0);
                        }
                        else
                            cerr << "Drawing an arc with an aperture different from a circle "
                                         "is forbidden by the Gerber standard; skipping."
                                      << endl;
                    }
                }
                else
                    cerr << "Circular arc requested but cirseg == NULL" << endl;
            }
            else if (currentNet->aperture_state == GERBV_APERTURE_STATE_FLASH) {
                cerr << "D03 during circular arc mode is forbidden by the Gerber "
                                "standard; skipping" << endl;
            }
        }
        else if (currentNet->interpolation == GERBV_INTERPOLATION_x10 ||
                 currentNet->interpolation == GERBV_INTERPOLATION_LINEARx01 || 
                 currentNet->interpolation == GERBV_INTERPOLATION_LINEARx001 ) {
            cerr << "Linear zoomed interpolation modes are not supported "
                         "(are them in the RS274X standard?)" << endl;
        }
        else //if (currentNet->interpolation != GERBV_INTERPOLATION_DELETED)
        {
            cerr << "Unrecognized interpolation mode" << endl;
        }
    }

    for (pair<const gerbv_layer_t *, gerberimporter_layer>& layer : layers)
    {
        for (pair<const coordinate_type, multi_linestring_type>& path : layer.second.paths)
        {
            simplify_paths(path.second);
        }
    }

    return generate_layers(layers, fill_closed_lines, cfactor, points_per_circle);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
GerberImporter::~GerberImporter()
{
    gerbv_destroy_project(project);
}
