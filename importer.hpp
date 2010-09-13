#ifndef IMPORTER_H
#define IMPORTER_H

#include <glibmm/ustring.h>
using Glib::ustring;

#include <glibmm/refptr.h>
#include <cairomm/cairomm.h>
#include <gdk/gdkcairo.h>

#include <boost/exception/all.hpp>
struct import_exception : virtual std::exception, virtual boost::exception {};
typedef boost::error_info<struct tag_my_info, ustring> errorstring;

//! pure virtual base class for importers.
class LayerImporter
{
public:
	virtual gdouble get_width() = 0;
	virtual gdouble get_height() = 0;
	virtual gdouble get_min_x() = 0;
	virtual gdouble get_max_x() = 0;
	virtual gdouble get_min_y() = 0;
	virtual gdouble get_max_y() = 0;

	virtual void render(Cairo::RefPtr<Cairo::ImageSurface> surface,
			    const guint dpi, const double xoff, const double yoff)
		throw (import_exception) = 0;
	
};

#endif // IMPORTER_H
