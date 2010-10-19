
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

#ifndef SURFACE_H
#define SURFACE_H

#include <boost/noncopyable.hpp>
#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <vector>
using std::vector;

#include <glibmm/refptr.h>
#include <gdkmm/pixbuf.h>
#include <glibmm/ustring.h>
using Glib::ustring;

#include "coord.hpp"
#include "mill.hpp"
#include "gerberimporter.hpp"

struct surface_exception : virtual std::exception, virtual boost::exception {};

class Surface : virtual public boost::noncopyable
{
public:
	Surface( guint dpi, ivalue_t min_x, ivalue_t max_x, ivalue_t min_y, ivalue_t max_y );
	void render( boost::shared_ptr<LayerImporter> importer) throw(import_exception);

	boost::shared_ptr< Surface > deep_copy();

	void save_debug_image();

	vector< shared_ptr<icoords> > get_toolpath( shared_ptr<RoutingMill> mill, bool mirror = false );
	ivalue_t get_width_in() { return max_x - min_x; };
	ivalue_t get_height_in() { return max_y - min_y; };

	void add_mask( shared_ptr<Surface>);

protected:
	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
	Cairo::RefPtr<Cairo::ImageSurface> cairo_surface;

	static const int procmargin = 10;

	const ivalue_t dpi;
	const ivalue_t min_x, max_x, min_y, max_y;
	const int zero_x, zero_y;

	void make_the_surface(uint width, uint height);

	// Image Processing Methods

	inline ivalue_t xpt2i( int xpt ) { return ivalue_t(xpt - zero_x) / ivalue_t(dpi); }
	inline ivalue_t ypt2i( int ypt ) { return ivalue_t(ypt - zero_y) / ivalue_t(dpi); }

	inline int xi2pt( ivalue_t xi ) { return int(xi * ivalue_t(dpi)) + zero_x; }
	inline int yi2pt( ivalue_t yi ) { return int(yi * ivalue_t(dpi)) + zero_y; }

	std::vector< std::pair<int,int> > fill_all_components();
	void fill_a_component(int x, int y, guint32 argb);
	uint grow_a_component(int x, int y, int& contentions);
	inline bool allow_grow(int x, int y, guint32 ownclr);

	void run_to_border(int& x, int& y);
	void calculate_outline(int x, int y, vector< std::pair<int,int> >& outside,
			       vector< std::pair<int,int> >& inside);

	// Misc. Functions
	static void opacify( Glib::RefPtr<Gdk::Pixbuf> pixbuf );

	guint32 clr;
	guint32 get_an_unused_color();
};


#endif // SURFACE_H
