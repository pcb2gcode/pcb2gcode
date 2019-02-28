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

#ifndef GERBERIMPORTER_H
#define GERBERIMPORTER_H


#include <exception>         // for exception
#include <gdk/gdk.h>         // for GdkColor
extern "C" {
#include <gerbv.h>           // for GERBV_RENDER_TYPE_CAIRO_NORMAL, gerbv_pr...
}
#include <glib.h>            // for gdouble, guint
#include <string>            // for string
#include "geometry.hpp"      // for multi_polygon_type_fp
#include "importer.hpp"      // for RasterLayerImporter, VectorialLayerImporter
namespace Cairo { class ImageSurface; }  // lines 32-32
namespace Cairo { template <class T_CppObject> class RefPtr; }  // lines 33-33

class gerber_exception: public std::exception {};

/******************************************************************************/
/*
 Importer for RS274-X Gerber files.

 GerberImporter is using libgerbv and hence features its suberb support for
 different file formats and gerber dialects.
 */
/******************************************************************************/
class GerberImporter: public RasterLayerImporter, public VectorialLayerImporter {
public:
    GerberImporter();
  bool load_file(const std::string& path);
    virtual ~GerberImporter();

    virtual gdouble get_min_x() const;
    virtual gdouble get_max_x() const;
    virtual gdouble get_min_y() const;
    virtual gdouble get_max_y() const;

    virtual void render(Cairo::RefPtr<Cairo::ImageSurface> surface,
                        const guint dpi, const double min_x,
                        const double min_y, const GdkColor& color = { 0xFFFFFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
                        const gerbv_render_types_t& renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL) const;
    virtual multi_polygon_type_fp render(bool fill_closed_lines, unsigned int points_per_circle = 30) const;

protected:
    enum Side { FRONT = 0, BACK = 1 } side;

private:
    gerbv_project_t* project;
};

#endif // GERBERIMPORTER_H
