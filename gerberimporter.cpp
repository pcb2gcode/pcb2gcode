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

#include <iostream>
#include "gerberimporter.hpp"
#include <boost/scoped_array.hpp>

/******************************************************************************/
/*
 */
/******************************************************************************/
GerberImporter::GerberImporter(const string path)
{
    project = gerbv_create_project();

    const char* cfilename = path.c_str();
    boost::scoped_array<char> filename(new char[strlen(cfilename) + 1]);
    strcpy(filename.get(), cfilename);

    gerbv_open_layer_from_filename(project, filename.get());
    if (project->file[0] == NULL)
        throw gerber_exception();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_width()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_x - project->file[0]->image->info->min_x;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_height()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_y - project->file[0]->image->info->min_y;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_min_x()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->min_x;

}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_max_x()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_x;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_min_y()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->min_y;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
gdouble GerberImporter::get_max_y()
{
    if (!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_y;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void GerberImporter::render(Cairo::RefPtr<Cairo::ImageSurface> surface, const guint dpi, const double min_x, const double min_y) throw (import_exception)
{
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

/******************************************************************************/
/*
 */
/******************************************************************************/
GerberImporter::~GerberImporter()
{
    gerbv_destroy_project(project);
}
