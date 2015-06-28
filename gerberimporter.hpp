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

#include <string>
using std::string;

#include "importer.hpp"

extern "C" {
#include <gerbv.h>
}

struct gerber_exception: virtual import_exception
{
};

/******************************************************************************/
/*
 Importer for RS274-X Gerber files.

 GerberImporter is using libgerbv and hence features its suberb support for
 different file formats and gerber dialects.
 */
/******************************************************************************/
class GerberImporter: virtual public LayerImporter
{
public:
    GerberImporter(const string path);

    virtual gdouble get_width();
    virtual gdouble get_height();
    virtual gdouble get_min_x();
    virtual gdouble get_max_x();
    virtual gdouble get_min_y();
    virtual gdouble get_max_y();

    virtual void render(Cairo::RefPtr<Cairo::ImageSurface> surface,
                        const guint dpi, const double min_x,
                        const double min_y) throw (import_exception);

    virtual ~GerberImporter();
protected:
private:

    gerbv_project_t* project;
};

#endif // GERBERIMPORTER_H
