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
typedef bg::strategy::transform::translate_transformer<coordinate_type_fp, 2, 2> translate;

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

gdouble GerberImporter::get_min_x() const {
  return project->file[0]->image->info->min_x;
}

gdouble GerberImporter::get_max_x() const {
  return project->file[0]->image->info->max_x;
}

gdouble GerberImporter::get_min_y() const {
  return project->file[0]->image->info->min_y;
}

gdouble GerberImporter::get_max_y() const {
  return project->file[0]->image->info->max_y;
}

void GerberImporter::render(Cairo::RefPtr<Cairo::ImageSurface> surface, const guint dpi, const double min_x, const double min_y,
                            const GdkColor& color) const {
  gerbv_render_info_t render_info;

  render_info.scaleFactorX = dpi;
  render_info.scaleFactorY = dpi;
  render_info.lowerLeftX = min_x;
  render_info.lowerLeftY = min_y;
  render_info.displayWidth = surface->get_width();
  render_info.displayHeight = surface->get_height();
  render_info.renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL;

  project->file[0]->color = color;

  Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surface);
  gerbv_render_layer_to_cairo_target(cr->cobj(), project->file[0], &render_info);

  /// @todo check wheter importing was successful
}

// Draw a regular polygon with outer diameter as specified and center.  The
// number of vertices is provided.  offset is an angle in degrees to the
// starting vertex of the shape.
multi_polygon_type_fp make_regular_polygon(point_type_fp center, coordinate_type_fp diameter, unsigned int vertices,
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
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::difference(lhs, rhs, ret);
  return ret;
}

template <typename polygon_type_t>
static inline bg::model::multi_polygon<polygon_type_t> operator+(const bg::model::multi_polygon<polygon_type_t>& lhs,
                                                                 const bg::model::multi_polygon<polygon_type_t>& rhs) {
  if (bg::area(rhs) <= 0) {
    return lhs;
  }
  bg::model::multi_polygon<polygon_type_t> ret;
  bg::union_(lhs, rhs, ret);
  return ret;
}

// Same as above but potentially puts a hole in the center.
multi_polygon_type_fp make_regular_polygon(point_type_fp center, coordinate_type_fp diameter, unsigned int vertices,
                                           coordinate_type_fp offset, coordinate_type_fp hole_diameter,
                                           unsigned int circle_points) {
  multi_polygon_type_fp ret;
  ret = make_regular_polygon(center, diameter, vertices, offset);

  if (hole_diameter > 0) {
    ret = ret - make_regular_polygon(center, hole_diameter, circle_points, 0);
  }
  return ret;
}

multi_polygon_type_fp make_rectangle(point_type_fp center, double width, double height,
                                     coordinate_type_fp hole_diameter, unsigned int circle_points) {
  const coordinate_type_fp x = center.x();
  const coordinate_type_fp y = center.y();

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
  linestring_type_fp line;
  line.push_back(point1);
  line.push_back(point2);
  bg::buffer(line, ret,
             bg::strategy::buffer::distance_symmetric<coordinate_type_fp>(height/2),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(0),
             bg::strategy::buffer::end_flat(),
             bg::strategy::buffer::point_circle(0));
  return ret;
}

multi_polygon_type_fp make_oval(point_type_fp center, coordinate_type_fp width, coordinate_type_fp height,
                                coordinate_type_fp hole_diameter, unsigned int circle_points) {
  point_type_fp start(center.x(), center.y());
  point_type_fp end(center.x(), center.y());
  if (width > height) {
    // The oval is more wide than tall.
    start.x(start.x() - (width - height)/2);
    end.x(end.x() + (width - height)/2);
  } else if (width < height) {
    // The oval is more tall than wide.
    start.y(start.y() - (height - width)/2);
    end.y(end.y() + (height - width)/2);
  } else {
    // This is just a circle.  Older boost doesn't handle a line with no length
    // though new boost does.
    return make_regular_polygon(center, width, circle_points, 0, hole_diameter, circle_points);
  }

  multi_polygon_type_fp oval;
  linestring_type_fp line;
  line.push_back(start);
  line.push_back(end);
  bg::buffer(line, oval,
             bg::strategy::buffer::distance_symmetric<coordinate_type_fp>(std::min(width, height)/2),
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

multi_polygon_type_fp linear_draw_rectangular_aperture(point_type_fp startpoint, point_type_fp endpoint, coordinate_type_fp width,
                                                       coordinate_type_fp height) {
  // It's the convex hull of all the corners of all the points.
  multi_point_type_fp all_points;
  for (const auto& p : {startpoint, endpoint}) {
    for (double w : {-1, 1}) {
      for (double h : {-1, 1}) {
        all_points.push_back(point_type_fp(p.x()+w*width/2, p.y()+h*height/2));
      }
    }
  }
  multi_polygon_type_fp hull;
  hull.resize(1);
  bg::convex_hull(all_points, hull[0]);
  return hull;
}

double get_angle(point_type_fp start, point_type_fp center, point_type_fp stop, bool clockwise) {
  double start_angle = atan2(start.y() - center.y(), start.x() - center.x());
  double  stop_angle = atan2( stop.y() - center.y(),  stop.x() - center.x());
  double delta_angle = stop_angle - start_angle;
  while (clockwise && delta_angle > 0) {
    delta_angle -= 2 * bg::math::pi<double>();
  }
  while (!clockwise && delta_angle < 0) {
    delta_angle += 2 * bg::math::pi<double>();
  }
  return delta_angle;
}

// delta_angle is in radians.  Positive signed is counterclockwise, like math.
linestring_type_fp circular_arc(const point_type_fp& start, const point_type_fp& stop,
                                point_type_fp center, const coordinate_type_fp& radius, const coordinate_type_fp& radius2,
                                double delta_angle, const bool& clockwise, const unsigned int& circle_points) {
  // We can't trust gerbv to calculate single-quadrant vs multi-quadrant
  // correctly so we must so it ourselves.
  bool definitely_sq = false;
  if (radius != radius2) {
    definitely_sq = true; // Definiltely single-quadrant.
  }
  if (start.x() == stop.x() && start.y() == stop.y()) {
    // Either 0 or 360, depending on mq/sq.
    if (definitely_sq) {
      delta_angle = 0;
    } else {
      if (std::abs(delta_angle) < bg::math::pi<double>()) {
        delta_angle = 0;
      } else {
        delta_angle = bg::math::pi<double>() * 2;
        if (clockwise) {
          delta_angle = -delta_angle;
        }
      }
    }
  } else {
    const auto signs_to_try = definitely_sq ? vector<double>{-1, 1} : vector<double>{1};
    const coordinate_type_fp i = std::abs(center.x() - start.x());
    const coordinate_type_fp j = std::abs(center.y() - start.y());
    delta_angle = get_angle(start, center, stop, clockwise);
    for (const double& i_sign : signs_to_try) {
      for (const double& j_sign : signs_to_try) {
        const point_type_fp current_center = point_type_fp(start.x() + i*i_sign, start.y() + j*j_sign);
        double new_angle = get_angle(start, current_center, stop, clockwise);
        if (std::abs(new_angle) > bg::math::pi<double>()) {
          continue; // Wrong side.
        }
        if (std::abs(bg::distance(start, current_center) - bg::distance(stop, current_center)) <
            std::abs(bg::distance(start,         center) - bg::distance(stop,         center))) {
          // This is closer to the center line so it's a better choice.
          delta_angle = new_angle;
          center = current_center;
        }
      }
    }
  }

  // Now delta_angle is between -2pi and 2pi and accurate and center is correct.
  const double start_angle = atan2(start.y() - center.y(), start.x() - center.x());
  const double stop_angle = start_angle + delta_angle;
  const coordinate_type_fp start_radius = bg::distance(start, center);
  const coordinate_type_fp stop_radius = bg::distance(stop, center);
  const unsigned int steps = ceil(std::abs(delta_angle) / (2 * bg::math::pi<double>()) * circle_points)
                             + 1; // One more for the end point.
  linestring_type_fp linestring;
  // First place the start;
  linestring.push_back(start);
  for (unsigned int i = 1; i < steps - 1; i++) {
    const double stop_weight = double(i) / (steps - 1);
    const double start_weight = 1 - stop_weight;
    const double current_angle = start_angle*start_weight + stop_angle*stop_weight;
    const double current_radius = start_radius*start_weight + stop_radius*stop_weight;
    linestring.push_back(point_type_fp(cos(current_angle) * current_radius + center.x(),
                                       sin(current_angle) * current_radius + center.y()));
  }
  linestring.push_back(stop);
  return linestring;
}

inline static void unsupported_polarity_throw_exception() {
  cerr << ("Non-positive image polarity is deprecated by the Gerber "
           "standard and unsupported; re-run pcb2gcode without the "
           "--vectorial flag") << endl;
  throw gerber_exception();
}

multi_polygon_type_fp generate_layers(vector<pair<const gerbv_layer_t *, multi_polygon_type_fp>>& layers,
                                      bool fill_rings, unsigned int points_per_circle) {
  multi_polygon_type_fp output;
  vector<ring_type_fp> rings;

  for (auto layer = layers.cbegin(); layer != layers.cend(); layer++) {
    const gerbv_polarity_t polarity = layer->first->polarity;
    const gerbv_step_and_repeat_t& stepAndRepeat = layer->first->stepAndRepeat;
    multi_polygon_type_fp draws = layer->second;

    // First duplicate in the x direction.
    auto original_draw = draws;
    for (int sr_x = 1; sr_x < stepAndRepeat.X; sr_x++) {
      multi_polygon_type_fp translated_draws;
      bg::transform(original_draw, translated_draws,
                    translate(stepAndRepeat.dist_X * sr_x, 0));
      draws = draws + translated_draws;
    }

    // Now duplicate in the y direction, with all the x duplicates in there already.
    original_draw = draws;
    for (int sr_y = 1; sr_y < stepAndRepeat.Y; sr_y++) {
      multi_polygon_type_fp translated_draws;
      bg::transform(original_draw, translated_draws,
                    translate(0, stepAndRepeat.dist_Y * sr_y));
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

multi_polygon_type_fp make_moire(const double * const parameters, unsigned int circle_points) {
  const point_type_fp center(parameters[0], parameters[1]);
  multi_polygon_type_fp moire;

  double crosshair_thickness = parameters[6];
  double crosshair_length = parameters[7];
  moire = moire + make_rectangle(center, crosshair_thickness, crosshair_length, 0, 0);
  moire = moire + make_rectangle(center, crosshair_length, crosshair_thickness, 0, 0);
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
    moire = moire + make_regular_polygon(center, external_diameter, circle_points, 0,
                                         internal_diameter, circle_points);
  }
  return moire;
}

multi_polygon_type_fp make_thermal(point_type_fp center, coordinate_type_fp external_diameter, coordinate_type_fp internal_diameter,
                                   coordinate_type_fp gap_width, unsigned int circle_points) {
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

map<int, multi_polygon_type_fp> generate_apertures_map(const gerbv_aperture_t * const apertures[], unsigned int circle_points) {
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
                                       parameters[0],
                                       circle_points,
                                       parameters[1],
                                       parameters[2],
                                       circle_points);
          break;
        case GERBV_APTYPE_RECTANGLE:
          input = make_rectangle(origin,
                                 parameters[0],
                                 parameters[1],
                                 parameters[2],
                                 circle_points);
          break;
        case GERBV_APTYPE_OVAL:
          input = make_oval(origin,
                            parameters[0],
                            parameters[1],
                            parameters[2],
                            circle_points);
          break;
        case GERBV_APTYPE_POLYGON:
          input = make_regular_polygon(origin,
                                       parameters[0],
                                       parameters[1],
                                       parameters[2],
                                       parameters[3],
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
                  mpoly = make_regular_polygon(point_type_fp(parameters[2], parameters[3]),
                                               parameters[1],
                                               circle_points,
                                               0);
                  polarity = parameters[0];
                  rotation = parameters[4];
                  break;
                case GERBV_APTYPE_MACRO_OUTLINE:
                  {
                    ring_type_fp ring;
                    for (unsigned int i = 0; i < round(parameters[1]) + 1; i++){
                      ring.push_back(point_type_fp(parameters[i * 2 + 2],
                                                   parameters [i * 2 + 3]));
                    }
                    bg::correct(ring);
                    mpoly = simplify_cutins(ring);
                  }
                  polarity = parameters[0];
                  rotation = parameters[(2 * int(round(parameters[1])) + 4)];
                  break;
                case GERBV_APTYPE_MACRO_POLYGON: // 4.12.4.6 Polygon, Primitve Code 5
                  mpoly = make_regular_polygon(point_type_fp(parameters[2], parameters[3]),
                                               parameters[4],
                                               parameters[1],
                                               0);
                  polarity = parameters[0];
                  rotation = parameters[5];
                  break;
                case GERBV_APTYPE_MACRO_MOIRE: // 4.12.4.7 Moire, Primitive Code 6
                  mpoly = make_moire(parameters, circle_points);
                  polarity = 1;
                  rotation = parameters[8];
                  break;
                case GERBV_APTYPE_MACRO_THERMAL: // 4.12.4.8 Thermal, Primitive Code 7
                  mpoly = make_thermal(point_type_fp(parameters[0], parameters[1]),
                                       parameters[2],
                                       parameters[3],
                                       parameters[4],
                                       circle_points);
                  polarity = 1;
                  rotation = parameters[5];
                  break;
                case GERBV_APTYPE_MACRO_LINE20: // 4.12.4.3 Vector Line, Primitive Code 20
                  mpoly = make_rectangle(point_type_fp(parameters[2], parameters[3]),
                                         point_type_fp(parameters[4], parameters[5]),
                                         parameters[1]);
                  polarity = parameters[0];
                  rotation = parameters[6];
                  break;
                case GERBV_APTYPE_MACRO_LINE21: // 4.12.4.4 Center Line, Primitive Code 21
                  mpoly = make_rectangle(point_type_fp(parameters[3], parameters[4]),
                                         parameters[1],
                                         parameters[2],
                                         0, 0);
                  polarity = parameters[0];
                  rotation = parameters[5];
                  break;
                case GERBV_APTYPE_MACRO_LINE22:
                  mpoly = make_rectangle(point_type_fp((parameters[3] + parameters[1] / 2),
                                                       (parameters[4] + parameters[2] / 2)),
                                         parameters[1],
                                         parameters[2],
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
             bg::strategy::buffer::distance_symmetric<coordinate_type_fp>(diameter / 2),
             bg::strategy::buffer::side_straight(),
             bg::strategy::buffer::join_round(points_per_circle),
                   bg::strategy::buffer::end_round(points_per_circle),
             bg::strategy::buffer::point_circle(points_per_circle));
  return ovals;
}


// Convert the gerber file into a multi_polygon_type_fp.  If fill_closed_lines is
// true, return all closed shapes without holes in them.  points_per_circle is
// the number of lines to use to appoximate circles.
multi_polygon_type_fp GerberImporter::render(bool fill_closed_lines, unsigned int points_per_circle) const {
  ring_type_fp region;
  unique_ptr<multi_polygon_type_fp> temp_mpoly (new multi_polygon_type_fp());
  bool contour = false; // Are we in contour mode?

  vector<pair<const gerbv_layer_t *, multi_polygon_type_fp>> layers(1);

  gerbv_image_t *gerber = project->file[0]->image;

  if (gerber->info->polarity != GERBV_POLARITY_POSITIVE) {
    unsupported_polarity_throw_exception();
  }

  const map<int, multi_polygon_type_fp> apertures_map = generate_apertures_map(gerber->aperture, points_per_circle);
  layers.front().first = gerber->netlist->layer;


  map<coordinate_type_fp, multi_linestring_type_fp> linear_circular_paths;
  for (gerbv_net_t *currentNet = gerber->netlist; currentNet; currentNet = currentNet->next) {
    const point_type_fp start (currentNet->start_x, currentNet->start_y);
    const point_type_fp stop (currentNet->stop_x, currentNet->stop_y);
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
            const double diameter = parameters[0];
            linestring_type_fp segment;
            segment.push_back(start);
            segment.push_back(stop);
            linear_circular_paths[diameter].push_back(segment);
            draws = draws + mpoly;
          } else if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_RECTANGLE) {
            mpoly = linear_draw_rectangular_aperture(start, stop, parameters[0],
                                                     parameters[1]);
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
        if (cirseg != NULL) {
          double delta_angle = (cirseg->angle1 - cirseg->angle2) * bg::math::pi<double>() / 180.0;
          if (currentNet->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR) {
            delta_angle = -delta_angle;
          }
          point_type_fp center(cirseg->cp_x, cirseg->cp_y);
          linestring_type_fp path = circular_arc(start, stop, center,
                                                 cirseg->width / 2,
                                                 cirseg->height / 2,
                                                 delta_angle,
                                                 currentNet->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR,
                                                 points_per_circle);
          if (contour) {
            if (region.empty()) {
              region.insert(region.end(), path.begin(), path.end());
            } else {
              region.insert(region.end(), path.begin() + 1, path.end());
            }
          } else {
            if (gerber->aperture[currentNet->aperture]->type == GERBV_APTYPE_CIRCLE) {
              const double diameter = parameters[0];
              for (size_t i = 1; i < path.size(); i++) {
                linestring_type_fp segment;
                segment.push_back(path[i-1]);
                segment.push_back(path[i]);
                linear_circular_paths[diameter].push_back(segment);
              }
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
               "(are they in the RS274X standard?)") << endl;
    } else { //if (currentNet->interpolation != GERBV_INTERPOLATION_DELETED)
      cerr << "Unrecognized interpolation mode" << endl;
    }
  }
  // If there are any unrendered circular paths, add them to the last layer.
  for (const auto& diameter_and_path : linear_circular_paths) {
    layers.back().second = layers.back().second + paths_to_shapes(diameter_and_path.first, diameter_and_path.second, points_per_circle);
  }
  linear_circular_paths.clear();
  auto result = generate_layers(layers, fill_closed_lines, points_per_circle);
  if (fill_closed_lines) {
    for (auto& p : result) {
      p.inners().clear();
    }
  }

  if (gerber->netlist->state->unit == GERBV_UNIT_MM) {
    // I don't believe that this ever happens because I think that gerbv
    // internally converts everything to inches.
    multi_polygon_type_fp scaled_result;
    bg::transform(result, scaled_result,
                  bg::strategy::transform::scale_transformer<coordinate_type_fp, 2, 2>(
                      scale/25.4, scale/25.4));
    return scaled_result;
  } else {
    multi_polygon_type_fp scaled_result;
    bg::transform(result, scaled_result,
                  bg::strategy::transform::scale_transformer<coordinate_type_fp, 2, 2>(
                      scale, scale));
    return scaled_result;
  }
}

GerberImporter::~GerberImporter() {
  gerbv_destroy_project(project);
}
