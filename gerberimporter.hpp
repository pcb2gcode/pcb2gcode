#ifndef GERBERIMPORTER_H
#define GERBERIMPORTER_H

#include <string>
using std::string;

#include "importer.hpp"

extern "C"
{
    #include <gerbv.h>
}

struct gerber_exception : virtual import_exception {};

//! Importer for RS274-X Gerber files.

/*! GerberImporter is using libgerbv and hence features
 *  its suberb support for different file formats and
 *  gerber dialects.
 */

class GerberImporter : virtual public LayerImporter
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
			const guint dpi, const double min_x, const double min_y)
	    throw (import_exception);

    virtual ~GerberImporter();
protected:
private:

    gerbv_project_t* project;
};

#endif // GERBERIMPORTER_H
