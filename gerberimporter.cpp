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
#include "eulerian_paths.hpp"

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

// Draw a regular polygon with outer diameter as specified and center.  The
// number of vertices is provided.  offset is an angle in degrees to the
// starting vertex of the shape.
multi_polygon_type_fp make_regular_polygon(point_type_fp center, coordinate_type diameter, unsigned int vertices,
                                           double offset) {
  double angle_step;

  angle_step = -2 * bg::math::pi<double>() / vertices;
  offset *= bg::math::pi<double>() / 180.0; // Convert to radians.

  ring_type_fp ring;
  for (unsigned int i = 0; i < vertices; i++) {
    ring.push_back(point_type_fp(cos(angle_step * i + offset) * diameter / 2 + center.x(),
                                 sin(angle_step * i + offset) * diameter / 2 + center.y()));
  }
  ring.push_back(ring.front()); // Don't forget to close the ring.
  multi_polygon_type_fp ret;
  bg::convert(ring, ret);
  return ret;
}

template <typename polygon_type_t>
static inline bg::model::multi_polygon<polygon_type_t> operator-(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const bg::model::multi_polygon<polygon_type_t>& rhs) {
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::difference(lhs, rhs, ret);
  return ret;
}

template <typename polygon_type_t>
static inline bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const bg::model::multi_polygon<polygon_type_t>& rhs) {
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::union_(lhs, rhs, ret);
  return ret;
}

// Same as above but potentially puts a hole in the center.
multi_polygon_type_fp make_regular_polygon(point_type_fp center, coordinate_type diameter, unsigned int vertices,
                                           coordinate_type offset, coordinate_type hole_diameter,
                                           unsigned int circle_points) {
  multi_polygon_type_fp ret;
  ret = make_regular_polygon(center, diameter, vertices, offset);

  if (hole_diameter > 0) {
    ret = ret - make_regular_polygon(center, hole_diameter, circle_points, 0);
  }
  return ret;
}

multi_polygon_type_fp make_rectangle(point_type_fp center, double width, double height,
                                     coordinate_type hole_diameter, unsigned int circle_points) {
  const coordinate_type x = center.x();
  const coordinate_type y = center.y();

  multi_polygon_type_fp ret;
  ret.resize(1);
  auto& polygon = ret.front();
  polygon.outer().push_back(point_type_fp(x - width / 2, y - height / 2));
  polygon.outer().push_back(point_type_fp(x - width / 2, y + height / 2));
  polygon.outer().push_back(point_type_fp(x + width / 2, y + height / 2));
  polygon.outer().push_back(point_type_fp(x + width / 2, y - height / 2));
  polygon.outer().push_back(polygon.outer().front());

  if (hole_diameter > 0) {
    ret = ret - make_regular_polygon(center, hole_diameter, circle_points, 0);
  }
  return ret;
}

multi_polygon_type_fp make_rectangle(point_type_fp point1, point_type_fp point2, double height) {
  multi_polygon_type_fp ret;
  ret.resize(1);
  auto& polygon = ret.front();
  const double distance = bg::distance(point1, point2);
  const double normalized_dy = (point2.y() - point1.y()) / distance;
  const double normalized_dx = (point2.x() - point1.x()) / distance;
  // Rotate the normalized vectors by 90 degrees ccw.
  const double dy = height / 2 * normalized_dx;
  const double dx = height / 2 * -normalized_dy;

  polygon.outer().push_back(point_type_fp(point1.x() - dx, point1.y() - dy));
  polygon.outer().push_back(point_type_fp(point1.x() + dx, point1.y() + dy));
  polygon.outer().push_back(point_type_fp(point2.x() + dx, point2.y() + dy));
  polygon.outer().push_back(point_type_fp(point2.x() - dx, point2.y() - dy));
  polygon.outer().push_back(polygon.outer().front());

  return ret;
}

multi_polygon_type_fp make_oval(point_type_fp center, coordinate_type width, coordinate_type height,
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
  linestring_type_fp line;
  line.push_back(start);
  line.push_back(end);
  bg::buffer(line, oval,
             bg::strategy::buffer::distance_symmetric<coordinate_type>(std::min(width, height)/2),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(circle_points),
             bg::strategy::buffer::end_round(circle_points),
             bg::strategy::buffer::point_circle(circle_points));

  if (hole_diameter > 0) {
    multi_polygon_type_fp hole = make_regular_polygon(center, hole_diameter, circle_points, 0);
    multi_polygon_type_fp hole_fp;
    bg::convert(hole, hole_fp);
    oval = oval - hole_fp;
  }
  multi_polygon_type_fp ret;
  bg::convert(oval, ret);
  return ret;
}

multi_polygon_type_fp linear_draw_rectangular_aperture(point_type_fp startpoint, point_type_fp endpoint, coordinate_type width,
                                                       coordinate_type height) {
  auto start_rect = make_rectangle(startpoint, width, height, 0, 0);
  auto end_rect = make_rectangle(endpoint, width, height, 0, 0);
  multi_polygon_type_fp both_rects;
  both_rects = start_rect + end_rect;
  multi_polygon_type_fp hull;
  hull.resize(1);
  bg::convex_hull(both_rects, hull[0]);
  return hull;
}

multi_polygon_type_fp linear_draw_circular_aperture(point_type_fp startpoint, point_type_fp endpoint,
                                                    coordinate_type radius, unsigned int circle_points) {
  multi_polygon_type_fp oval;
  linestring_type_fp line;
  line.push_back(startpoint);
  line.push_back(endpoint);
  bg::buffer(line, oval,
             bg::strategy::buffer::distance_symmetric<coordinate_type>(radius),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(circle_points),
             bg::strategy::buffer::end_round(circle_points),
             bg::strategy::buffer::point_circle(circle_points));
  return oval;
}

void GerberImporter::circular_arc(point_type_fp center, coordinate_type radius,
                                  double angle1, double angle2, unsigned int circle_points,
                                  linestring_type_fp& linestring)
{
  const unsigned int steps = ceil((angle2 - angle1) / (2 * bg::math::pi<double>()) * circle_points);
  const double angle_step = (angle2 - angle1) / steps;
    
  for (unsigned int i = 0; i < steps; i++)
  {
    const double angle = angle1 + i * angle_step;

    linestring.push_back(point_type_fp(cos(angle) * radius + center.x(),
                                       sin(angle) * radius + center.y()));
  }
    
  linestring.push_back(point_type_fp(cos(angle2) * radius + center.x(),
                                     sin(angle2) * radius + center.y()));
}

inline static void unsupported_polarity_throw_exception() {
  cerr << ("Non-positive image polarity is deprecated by the Gerber "
           "standard and unsupported; re-run pcb2gcode without the "
           "--vectorial flag") << endl;
  throw gerber_exception();
}

multi_polygon_type_fp generate_layers(vector<pair<const gerbv_layer_t *, multi_polygon_type_fp>>& layers,
                                      bool fill_rings, coordinate_type cfactor, unsigned int points_per_circle) {
  multi_polygon_type_fp output;
  vector<ring_type_fp> rings;

  for (auto layer = layers.cbegin(); layer != layers.cend(); layer++) {
    unique_ptr<multi_polygon_type_fp> temp_mpoly (new multi_polygon_type_fp());
    const gerbv_polarity_t polarity = layer->first->polarity;
    const gerbv_step_and_repeat_t& stepAndRepeat = layer->first->stepAndRepeat;
    multi_polygon_type_fp draws = layer->second;

    // First duplicate in the x direction.
    auto original_draw = draws;
    for (int sr_x = 1; sr_x < stepAndRepeat.X; sr_x++) {
      multi_polygon_type_fp translated_draws;
      bg::transform(original_draw, translated_draws,
                    translate(stepAndRepeat.dist_X * sr_x * cfactor, 0));
      draws = draws + translated_draws;
    }

    // Now duplicate in the y direction, with all the x duplicates in there already.
    original_draw = draws;
    for (int sr_y = 1; sr_y < stepAndRepeat.Y; sr_y++) {
      multi_polygon_type_fp translated_draws;
      bg::transform(draws, translated_draws,
                    translate(0, stepAndRepeat.dist_Y * sr_y * cfactor));
      draws = draws + translated_draws;
    }

    if (polarity == GERBV_POLARITY_DARK) {
      output = output + draws;
    } else if (polarity == GERBV_POLARITY_CLEAR) {
      output = output - draws;
    } else {
      unsupported_polarity_throw_exception();
    }
  }

  return output;
}

multi_polygon_type_fp make_moire(const double * const parameters, unsigned int circle_points,
                                 coordinate_type cfactor) {
  const point_type_fp center(parameters[0] * cfactor, parameters[1] * cfactor);
  multi_polygon_type_fp moire;

  double crosshair_thickness = parameters[6];
  double crosshair_length = parameters[7];
  moire = moire + make_rectangle(center, crosshair_thickness * cfactor, crosshair_length * cfactor, 0, 0);
  moire = moire + make_rectangle(center, crosshair_length * cfactor, crosshair_thickness * cfactor, 0, 0);
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
    moire = moire + make_regular_polygon(center, external_diameter * cfactor, circle_points, 0,
                                         internal_diameter * cfactor, circle_points);
  }
  return moire;
}

multi_polygon_type_fp make_thermal(point_type_fp center, coordinate_type external_diameter, coordinate_type internal_diameter,
                                   coordinate_type gap_width, unsigned int circle_points) {
  multi_polygon_type_fp ring = make_regular_polygon(center, external_diameter, circle_points,
                                                    0, internal_diameter, circle_points);

  multi_polygon_type_fp rect1 = make_rectangle(center, gap_width, 2 * external_diameter, 0, 0);
  multi_polygon_type_fp rect2 = make_rectangle(center, 2 * external_diameter, gap_width, 0, 0);
  return ring - rect1 - rect2;
}

// Look through ring for crossing points and snip them out of the input so that
// the return value is a series of rings such that no ring has the same point in
// it twice.
vector<ring_type_fp> get_all_rings(const ring_type_fp& ring) {
  for (auto start = ring.cbegin(); start != ring.cend(); start++) {
    for (auto end = std::next(start); end != ring.cend(); end++) {
      if (bg::equals(*start, *end)) {
        if (start == ring.cbegin() && end == std::prev(ring.cend())) {
          continue; // This is just the entire ring, no need to try to recurse here.
        }
        ring_type_fp inner_ring(start, end); // Build the ring that we've found.
        inner_ring.push_back(inner_ring.front()); // Close the ring.

        // Make a ring from the rest of the points.
        ring_type_fp outer_ring(ring.cbegin(), start);
        outer_ring.insert(outer_ring.end(), end, ring.cend());
        // Recurse on outer and inner and put together.
        vector<ring_type_fp> all_rings = get_all_rings(outer_ring);
        vector<ring_type_fp> all_inner_rings = get_all_rings(inner_ring);
        all_rings.insert(all_rings.end(), all_inner_rings.cbegin(), all_inner_rings.cend());
        return all_rings;
      }
    }
  }
  // No points repeated so just return the original without recursion.
  return vector<ring_type_fp>{ring};
}

multi_polygon_type_fp simplify_cutins(const ring_type_fp& ring) {
  const auto area = bg::area(ring); // Positive if the original ring is clockwise, otherwise negative.
  vector<ring_type_fp> all_rings = get_all_rings(ring);
  multi_polygon_type_fp ret;
  for (auto r : all_rings) {
    const auto this_area = bg::area(r);
    if (r.size() < 4 || this_area == 0) {
      continue; // No area so ignore it.
    }
    if (this_area * area > 0) {
      multi_polygon_type_fp temp_ret;
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
      multi_polygon_type_fp temp_ret;
      auto correct_r = r;
      bg::correct(correct_r);
      bg::difference(ret, correct_r, temp_ret);
      ret = temp_ret;
    }
  }
  return ret;
}

map<int, multi_polygon_type_fp> generate_apertures_map(const gerbv_aperture_t * const apertures[], unsigned int circle_points, coordinate_type cfactor) {
  const point_type_fp origin (0, 0);
  map<int, multi_polygon_type_fp> apertures_map;
  for (int i = 0; i < APERTURE_MAX; i++) {
    const gerbv_aperture_t * const aperture = apertures[i];

    if (aperture) {
      const double * const parameters = aperture->parameter;
      multi_polygon_type_fp input;
      multi_polygon_type_fp output;

      switch (aperture->type) {
        case GERBV_APTYPE_NONE:
          continue;

        case GERBV_APTYPE_CIRCLE:
          input = make_regular_polygon(origin,
                                       parameters[0] * cfactor,
                                       circle_points,
                                       parameters[1] * cfactor,
                                       parameters[2] * cfactor,
                                       circle_points);
          break;
        case GERBV_APTYPE_RECTANGLE:
          input = make_rectangle(origin,
                                 parameters[0] * cfactor,
                                 parameters[1] * cfactor,
                                 parameters[2] * cfactor,
                                 circle_points);
          break;
        case GERBV_APTYPE_OVAL:
          input = make_oval(origin,
                            parameters[0] * cfactor,
                            parameters[1] * cfactor,
                            parameters[2] * cfactor,
                            circle_points);
          break;
        case GERBV_APTYPE_POLYGON:
          input = make_regular_polygon(origin,
                                       parameters[0] * cfactor,
                                       parameters[1] * cfactor,
                                       parameters[2] * cfactor,
                                       parameters[3] * cfactor,
                                       circle_points);
          break;
        case GERBV_APTYPE_MACRO:
          if (aperture->simplified) {
            // I thikn that this means that the marco's variables are substitued.
            const gerbv_simplified_amacro_t *simplified_amacro = aperture->simplified;

            while (simplified_amacro) {
              const double * const parameters = simplified_amacro->parameter;
              double rotation;
              int polarity;
              multi_polygon_type_fp mpoly;
              multi_polygon_type_fp mpoly_rotated;

              switch (simplified_amacro->type) {
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
                  mpoly = make_regular_polygon(point_type_fp(parameters[2] * cfactor, parameters[3] * cfactor),
                                               parameters[1] * cfactor,
                                               circle_points,
                                               0);
                  polarity = parameters[0];
                  rotation = parameters[4];
                  break;
                case GERBV_APTYPE_MACRO_OUTLINE:
                  {
                    ring_type_fp ring;
                    for (unsigned int i = 0; i < round(parameters[1]) + 1; i++){
                      ring.push_back(point_type_fp(parameters[i * 2 + 2] * cfactor,
                                                   parameters [i * 2 + 3] * cfactor));
                    }
                    bg::correct(ring);
                    mpoly = simplify_cutins(ring);
                  }
                  polarity = parameters[0];
                  rotation = parameters[(2 * int(round(parameters[1])) + 4)];
                  break;
                case GERBV_APTYPE_MACRO_POLYGON: // 4.12.4.6 Polygon, Primitve Code 5
                  mpoly = make_regular_polygon(point_type_fp(parameters[2] * cfactor, parameters[3] * cfactor),
                                               parameters[4] * cfactor,
                                               parameters[1],
                                               0);
                  polarity = parameters[0];
                  rotation = parameters[5];
                  break;
                case GERBV_APTYPE_MACRO_MOIRE: // 4.12.4.7 Moire, Primitive Code 6
                  mpoly = make_moire(parameters, circle_points, cfactor);
                  polarity = 1;
                  rotation = parameters[8];
                  break;
                case GERBV_APTYPE_MACRO_THERMAL: // 4.12.4.8 Thermal, Primitive Code 7
                  mpoly = make_thermal(point_type_fp(parameters[0] * cfactor, parameters[1] * cfactor),
                                       parameters[2] * cfactor,
                                       parameters[3] * cfactor,
                                       parameters[4] * cfactor,
                                       circle_points);
                  polarity = 1;
                  rotation = parameters[5];
                  break;
                case GERBV_APTYPE_MACRO_LINE20: // 4.12.4.3 Vector Line, Primitive Code 20
                  mpoly = make_rectangle(point_type_fp(parameters[2] * cfactor, parameters[3] * cfactor),
                                         point_type_fp(parameters[4] * cfactor, parameters[5] * cfactor),
                                         parameters[1] * cfactor);
                  polarity = parameters[0];
                  rotation = parameters[6];
                  break;
                case GERBV_APTYPE_MACRO_LINE21: // 4.12.4.4 Center Line, Primitive Code 21
                  mpoly = make_rectangle(point_type_fp(parameters[3] * cfactor, parameters[4] * cfactor),
                                         parameters[1] * cfactor,
                                         parameters[2] * cfactor,
                                         0, 0);
                  polarity = parameters[0];
                  rotation = parameters[5];
                  break;
                case GERBV_APTYPE_MACRO_LINE22:
                  mpoly = make_rectangle(point_type_fp((parameters[3] + parameters[1] / 2) * cfactor,
                                                       (parameters[4] + parameters[2] / 2) * cfactor),
                                         parameters[1] * cfactor,
                                         parameters[2] * cfactor,
                                         0, 0);
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

              if (polarity == 0) {
                bg::difference(input, mpoly_rotated, output);
              } else {
                bg::union_(input, mpoly_rotated, output);
              }
              input = output;
              simplified_amacro = simplified_amacro->next;
            }
          } else {
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
      apertures_map[i] = input;
    }
  }
  return apertures_map;
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

struct PointLessThan {
  bool operator()(const point_type_fp& a, const point_type_fp& b) const {
    return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
  }
};


multi_polygon_type_fp paths_to_shapes(const coordinate_type_fp& diameter, const multi_linestring_type_fp& paths, unsigned int points_per_circle) {
  // This converts the many small line segments into the longest paths possible.
  multi_linestring_type_fp euler_paths =
      eulerian_paths::get_eulerian_paths<point_type_fp, linestring_type_fp, multi_linestring_type_fp, PointLessThan>(
          paths);
  multi_polygon_type_fp ovals;
  // This converts the long paths into a shape with thickness equal to the specified diameter.
  bg::buffer(euler_paths, ovals,
             bg::strategy::buffer::distance_symmetric<coordinate_type>(diameter / 2),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(points_per_circle),
                   bg::strategy::buffer::end_round(points_per_circle),
             bg::strategy::buffer::point_circle(points_per_circle));
  return ovals;
}


// Convert the gerber file into a multi_polygon_type_fp.  If fill_closed_lines is
// true, return all closed shapes without holes in them.  points_per_circle is
// the number of lines to use to appoximate circles.
multi_polygon_type_fp GerberImporter::render(bool fill_closed_lines, unsigned int points_per_circle) {
  ring_type_fp region;
  coordinate_type cfactor;
  unique_ptr<multi_polygon_type_fp> temp_mpoly (new multi_polygon_type_fp());
  bool contour = false; // Are we in contour mode?

  vector<pair<const gerbv_layer_t *, multi_polygon_type_fp>> layers(1);

  gerbv_image_t *gerber = project->file[0]->image;

  if (gerber->info->polarity != GERBV_POLARITY_POSITIVE) {
    unsupported_polarity_throw_exception();
  }

  if (gerber->netlist->state->unit == GERBV_UNIT_MM) {
    cfactor = scale / 25.4;
  } else {
    cfactor = scale;
  }

  const map<int, multi_polygon_type_fp> apertures_map = generate_apertures_map(gerber->aperture, points_per_circle, cfactor);
  layers.front().first = gerber->netlist->layer;


  map<coordinate_type_fp, multi_linestring_type_fp> linear_circular_paths;
  for (gerbv_net_t *currentNet = gerber->netlist; currentNet; currentNet = currentNet->next) {
    const point_type_fp start (currentNet->start_x * cfactor, currentNet->start_y * cfactor);
    const point_type_fp stop (currentNet->stop_x * cfactor, currentNet->stop_y * cfactor);
    const double * const parameters = gerber->aperture[currentNet->aperture]->parameter;
    multi_polygon_type_fp mpoly;

    if (!layers_equivalent(currentNet->layer, layers.back().first)) {
      // About to start a new layer, render all the linear_circular_paths so far.
      for (const auto& diameter_and_path : linear_circular_paths) {
        layers.back().second = layers.back().second + paths_to_shapes(diameter_and_path.first, diameter_and_path.second, points_per_circle);
      }
      linear_circular_paths.clear();
      layers.resize(layers.size() + 1);
      layers.back().first = currentNet->layer;
    }

    multi_polygon_type_fp& draws = layers.back().second;

    if (currentNet->interpolation == GERBV_INTERPOLATION_LINEARx1) {
      if (currentNet->aperture_state == GERBV_APERTURE_STATE_ON) {
        if (contour) {
          if (region.empty()) {
            bg::append(region, start);
          }
          bg::append(region, stop);
        } else {
          if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_CIRCLE) {
            // These are common and too slow to merge one by one so we put them
            // all together and then do one big union at the end.
            const double diameter = parameters[0] * cfactor;
            linestring_type_fp segment;
            segment.push_back(start);
            segment.push_back(stop);
            linear_circular_paths[diameter].push_back(segment);
            //mpoly = linear_draw_circular_aperture(start, stop, diameter/2, points_per_circle);
            /*
              multi_polygon_type_fp for_viewing = mpoly;
              for (auto& p : for_viewing) {
              for (auto& point : p.outer()) {
              point.x(point.x()/10000);
              point.y(point.y()/10000);
              }
              for (auto& i : p.inners()) {
              for (auto& point : i) {
              point.x(point.x()/10000);
              point.y(point.y()/10000);
              }
              }
              }
              std::cout << bg::wkt(for_viewing) << std::endl;
            */
            draws = draws + mpoly;
          } else if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_RECTANGLE) {
            mpoly = linear_draw_rectangular_aperture(start, stop, parameters[0] * cfactor,
                                                     parameters[1] * cfactor);
            draws = draws + mpoly;
          } else {
            cerr << ("Drawing with an aperture different from a circle "
                     "or a rectangle is forbidden by the Gerber standard; skipping.")
                 << endl;
          }
        }
      } else if (currentNet->aperture_state == GERBV_APERTURE_STATE_FLASH) {
        if (contour) {
          cerr << ("D03 during contour mode is forbidden by the Gerber "
                   "standard; skipping") << endl;
        } else {
          const auto aperture_mpoly = apertures_map.find(currentNet->aperture);

          if (aperture_mpoly != apertures_map.end()) {
            bg::transform(aperture_mpoly->second, mpoly, translate(stop.x(), stop.y()));
          } else {
            cerr << "Macro aperture " << currentNet->aperture <<
                " not found in macros list; skipping" << endl;
          }
          draws = draws + mpoly;
        }
      } else if (currentNet->aperture_state == GERBV_APERTURE_STATE_OFF) {
        if (contour) {
          bg::append(region, stop);
          draws = draws + simplify_cutins(region);
          region.clear();
        }
      } else {
        cerr << "Unrecognized aperture state: skipping" << endl;
      }
    } else if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_START) {
      contour = true;
    } else if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_END) {
      contour = false;
      draws = draws + simplify_cutins(region);
      region.clear();
    } else if (currentNet->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR ||
               currentNet->interpolation == GERBV_INTERPOLATION_CCW_CIRCULAR) {
      if (currentNet->aperture_state == GERBV_APERTURE_STATE_ON) {
        const gerbv_cirseg_t * const cirseg = currentNet->cirseg;
        linestring_type_fp path;

        if (cirseg != NULL) {
          double angle1;
          double angle2;

          if (currentNet->interpolation == GERBV_INTERPOLATION_CCW_CIRCULAR) {
            angle1 = cirseg->angle1;
            angle2 = cirseg->angle2;
          } else {
            angle1 = cirseg->angle2;
            angle2 = cirseg->angle1;
          }

          circular_arc(point_type_fp(cirseg->cp_x * cfactor, cirseg->cp_y * cfactor),
                       cirseg->width * cfactor / 2,
                       angle1 * bg::math::pi<double>() / 180.0,
                       angle2 * bg::math::pi<double>() / 180.0,
                       points_per_circle,
                       path);

          if (contour) {
            if (region.empty()) {
              copy(path.begin(), path.end(), region.end());
            } else {
              copy(path.begin() + 1, path.end(), region.end());
            }
          } else {
            if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_CIRCLE) {
              //const double diameter = parameters[0] * cfactor;
              // TODO: Support circular arcs.
            } else {
              cerr << ("Drawing an arc with an aperture different from a circle "
                       "is forbidden by the Gerber standard; skipping.")
                   << endl;
            }
          }
        } else {
          cerr << "Circular arc requested but cirseg == NULL" << endl;
        }
      } else if (currentNet->aperture_state == GERBV_APERTURE_STATE_FLASH) {
        cerr << "D03 during circular arc mode is forbidden by the Gerber "
            "standard; skipping" << endl;
      }
    } else if (currentNet->interpolation == GERBV_INTERPOLATION_x10 ||
               currentNet->interpolation == GERBV_INTERPOLATION_LINEARx01 || 
               currentNet->interpolation == GERBV_INTERPOLATION_LINEARx001 ) {
      cerr << ("Linear zoomed interpolation modes are not supported "
               "(are them in the RS274X standard?)") << endl;
    } else { //if (currentNet->interpolation != GERBV_INTERPOLATION_DELETED)
      cerr << "Unrecognized interpolation mode" << endl;
    }
  }
  // If there are any unrendered circular paths, add them to the last layer.
  for (const auto& diameter_and_path : linear_circular_paths) {
    layers.back().second = layers.back().second + paths_to_shapes(diameter_and_path.first, diameter_and_path.second, points_per_circle);
  }
  linear_circular_paths.clear();
  auto result = generate_layers(layers, fill_closed_lines, cfactor, points_per_circle);
  if (fill_closed_lines) {
    for (auto& p : result) {
      p.inners().clear();
    }
  }
  /*multi_polygon_type_fp for_viewing = *result;
    for (auto& p : for_viewing) {
    for (auto& point : p.outer()) {
    point.x(point.x()/10000);
    point.y(point.y()/10000);
    }
    for (auto& i : p.inners()) {
    for (auto& point : i) {
    point.x(point.x()/10000);
    point.y(point.y()/10000);
    }
    }
    }
    std::cout << bg::wkt(for_viewing) << std::endl;*/
  return result;
}

GerberImporter::~GerberImporter() {
  gerbv_destroy_project(project);
}
