#include "gerberimporter.hpp"

#include <boost/scoped_array.hpp>

GerberImporter::GerberImporter(const string path)
{
    project = gerbv_create_project();

    const char* cfilename = path.c_str();
    boost::scoped_array<char> filename( new char[strlen(cfilename) + 1] );
    strcpy(filename.get(), cfilename);

    gerbv_open_layer_from_filename(project, filename.get());
    if( project->file[0] == NULL)
        throw gerber_exception();
}

gdouble
GerberImporter::get_width()
{
    if(!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_x - project->file[0]->image->info->min_x;
}

gdouble
GerberImporter::get_height()
{
    if(!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_y - project->file[0]->image->info->min_y;
}

gdouble
GerberImporter::get_min_x()
{
    if(!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->min_x;

}

gdouble
GerberImporter::get_max_x()
{
    if(!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_x;
}

gdouble
GerberImporter::get_min_y()
{
    if(!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->min_y;
}

gdouble
GerberImporter::get_max_y()
{
    if(!project || !project->file[0])
        throw gerber_exception();

    return project->file[0]->image->info->max_y;
}

#include <iostream>

void
GerberImporter::render(Cairo::RefPtr<Cairo::ImageSurface> surface,
		       const guint dpi, const double min_x, const double min_y)
	throw (import_exception)
{
    gerbv_render_info_t render_info;

    render_info.scaleFactorX  = dpi;
    render_info.scaleFactorY  = dpi;
    render_info.lowerLeftX    = min_x;
    render_info.lowerLeftY    = min_y;
    render_info.displayWidth  = surface->get_width();
    render_info.displayHeight = surface->get_height();
    render_info.renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL;

    GdkColor color_saturated_white = { 0xFFFFFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
    project->file[0]->color = color_saturated_white;

    cairo_t* cr = cairo_create( surface->cobj() );
    gerbv_render_layer_to_cairo_target( cr, project->file[0], &render_info );
    
    cairo_destroy(cr);

    /// @todo check wheter importing was successful
}

GerberImporter::~GerberImporter()
{
    gerbv_destroy_project(project);
}
