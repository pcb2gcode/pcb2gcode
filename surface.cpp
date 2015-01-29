/*!\defgroup SURFACE*/
/******************************************************************************/
/*!
 \file       surface.cpp
 \brief

 \version
 04.08.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Started documenting the code for doxygen processing.
 - Formatted the code with the Eclipse code styler (Style: K&R).

 \version
 1.1.4 - 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others

 \copyright  pcb2gcode is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 pcb2gcode is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.

 \ingroup    SURFACE
 */
/******************************************************************************/

#include "surface.hpp"
using std::pair;

// color definitions for the ARGB32 format used

#define OPAQUE 0xFF000000
#define RED 0xFF0000FF
#define GREEN 0xFF00FF00
#define BLUE 0xFFFF0000
#define WHITE ( RED | GREEN | BLUE )
// while equal by value, OPAQUE is used for |-ing and BLACK for setting or comparison
#define BLACK ( RED & GREEN & BLUE )

/******************************************************************************/
/*
 */
/******************************************************************************/
void Surface::make_the_surface(uint width, uint height) {
	pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, width, height);

	cairo_surface = Cairo::ImageSurface::create(pixbuf->get_pixels(),
			Cairo::FORMAT_ARGB32, width, height, pixbuf->get_rowstride());
}

#include <boost/foreach.hpp>
#include <iostream>
using std::cerr;
using std::endl;

#define PRC(x) *(reinterpret_cast<guint32*>(x))

/******************************************************************************/
/*
 */
/******************************************************************************/
Surface::Surface(guint dpi, ivalue_t min_x, ivalue_t max_x, ivalue_t min_y,
		ivalue_t max_y, string outputdir) :
		dpi(dpi), min_x(min_x), max_x(max_x), min_y(min_y), max_y(max_y), zero_x(
				-min_x * (ivalue_t) dpi + (ivalue_t) procmargin), zero_y(
				-min_y * (ivalue_t) dpi + (ivalue_t) procmargin), clr(32), outputdir(outputdir) {
	guint8* pixels;
	int stride;
	make_the_surface((max_x - min_x) * dpi + 2 * procmargin,
			(max_y - min_y) * dpi + 2 * procmargin);
	usedcolors.push_back(BLACK);
	usedcolors.push_back(WHITE);

	/* "Note that the buffer is not cleared; you will have to fill it completely yourself." */
	//printf("clearing\n");
	pixels = cairo_surface->get_data();
	stride = cairo_surface->get_stride();
	for (int y = 0; y < pixbuf->get_height(); y++) {
		for (int x = 0; x < pixbuf->get_width(); x++) {
			PRC(pixels + x*4 + y*stride) = BLACK;
		}
	}
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Surface::render(boost::shared_ptr<LayerImporter> importer)
		throw (import_exception) {
	importer->render(cairo_surface, dpi,
			min_x - static_cast<ivalue_t>(procmargin) / dpi,
			min_y - static_cast<ivalue_t>(procmargin) / dpi);
}

#include <iostream>
using std::cout;
using std::endl;
using std::list;

/******************************************************************************/
/*
 */
/******************************************************************************/
double distancePointLine(const icoordpair &x, const icoordpair &la,
		const icoordpair &lb) {
	icoordpair nab; //normal vector to a-b= {-ab_y,ab_x}
	nab.first = -(la.second - lb.second);
	nab.second = (la.first - lb.first);
	double lnab = sqrt(nab.first * nab.first + nab.second * nab.second);
	double skalar; //product

	skalar = nab.first * (x.first - la.first)
			+ nab.second * (x.second - la.second);
	return fabs(skalar / lnab);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void simplifypath(shared_ptr<icoords> outline, double accuracy) {
	//take two points of the path
	// and their interconnecting path.
	// if the distance between all intermediate points and this line is smaller
	// than the accuracy, all the points in between can be removed..
	bool change;
	int lasterased = 0;
	const bool debug = false;
	std::list<icoordpair> l;
	for (int i = 0; i < outline->size(); i++) {
		icoordpair &ii = (*outline)[i];
		l.push_back(ii);
	}

	if (debug)
		cerr << "outline size:" << outline->size() << endl;
	int pos = 0;
	do //cycle until no two points can be combined..
	{
		change = false;

		list<icoordpair>::iterator a = l.begin();
		do {
			list<icoordpair>::iterator b, c;
			b = a;
			b++;
			c = b;
			c++;
			if ((b == l.end()))
				break;
			double d = distancePointLine(*b, *a, *c);
			if ((d < accuracy)) {

				if (debug)
					cerr << "erasing at" << pos << " of " << l.size() << " d="
							<< d << endl;
				a = l.erase(b);
				change = true;
			} else
				a = b;
			pos++;
		} while (a != l.end());
		//change=false;
	} while (change);

	if (debug)
		cout << "copying" << endl;
	outline->resize(0);
	for (list<icoordpair>::iterator a = l.begin(); a != l.end(); a++)
		outline->push_back(*a);
	if (debug)
		cerr << "outline size:" << outline->size() << endl;

}

/******************************************************************************/
/*
 */
/******************************************************************************/
vector<shared_ptr<icoords> > Surface::get_toolpath(shared_ptr<RoutingMill> mill,
		bool mirrored, bool mirror_absolute) {
	Isolator* iso = dynamic_cast<Isolator*>(mill.get());
	int extra_passes = iso ? iso->extra_passes : 0;

	coords components = fill_all_components();

	int added = -1;
	int contentions = 0;
	int grow = mill->tool_diameter / 2 * dpi;
	ivalue_t double_mirror_axis = mirror_absolute ? 0 : (min_x + max_x);

	vector<shared_ptr<icoords> > toolpath;

	for (int pass = 0; pass <= extra_passes && added != 0; pass++) {
		for (int i = 0; i < grow && added != 0; i++) {
			added = 0;

			BOOST_FOREACH( coordpair c, components ) {
				added += grow_a_component(c.first, c.second, contentions);
			}
		}

		coords inside, outside;

		BOOST_FOREACH( coordpair c, components ) {
			calculate_outline(c.first, c.second, outside, inside);
			inside.clear();

			shared_ptr<icoords> outline(new icoords());

			// i'm not sure wheter this is the right place to do this...
			// that "mirrored" flag probably is a bad idea.
			BOOST_FOREACH( coordpair c, outside ) {
				outline->push_back(
						icoordpair(
								// tricky calculations
								mirrored ?
										(double_mirror_axis - xpt2i(c.first)) :
										xpt2i(c.first),
								min_y + max_y - ypt2i(c.second)));
			}

			if (0)
				simplifypath(outline, 0.005);
			outside.clear();
			toolpath.push_back(outline);
		}
	}

	if (contentions) {
		cerr << "\nWarning: pcb2gcode hasn't been able to fulfill all"
				<< " clearance requirements and tried a best effort approach"
				<< " instead. You may want to check the g-code output and"
				<< " possibly use a smaller milling width.\n";
	}

	save_debug_image("traced");
	return toolpath;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
guint32 Surface::get_an_unused_color() {
	bool badcol;
	do {
		badcol = false;
		clr = rand();

		for (int i = 0; i < usedcolors.size(); i++) {
			if (usedcolors[i] == clr) {
				badcol = true;
				break;
			}
		}
	} while (badcol);
	usedcolors.push_back(clr);
	return clr;
}

/******************************************************************************/
/*
 \brief		try to find white pixels, aka uncolored pixels,
 and do some floodfilling with a random color based on them.
 \retval	list floodfill-seed points
 */
/******************************************************************************/
std::vector<std::pair<int, int> > Surface::fill_all_components() {
	std::vector<pair<int, int> > components;
	int max_x = cairo_surface->get_width() - 1;
	int max_y = cairo_surface->get_height() - 1;
	guint8* pixels = cairo_surface->get_data();
	int stride = cairo_surface->get_stride();

	for (int y = 0; y <= max_y; y++) {
		for (int x = 0; x <= max_x; x++) {
			if ((PRC(pixels + x*4 + y*stride) | OPAQUE) == WHITE) {
				components.push_back(pair<int, int>(x, y));
				fill_a_component(x, y, get_an_unused_color());
			}
		}
	}

	return components;
}

#include <stack>

/******************************************************************************/
/*
 \brief	fill_a_component does not do any image boundary checks out of performance reasons.
 */
/******************************************************************************/
void Surface::fill_a_component(int x, int y, guint32 argb) {
	guint32 newclr = argb;

	guint8* pixels = cairo_surface->get_data();
	int stride = cairo_surface->get_stride();

	guint8* here = pixels + x * 4 + y * stride;
	guint8* maxhere = pixels + (cairo_surface->get_width() - 1) * 4
			+ (cairo_surface->get_height() - 1) * stride;

	guint32 ownclr = PRC(here);

	std::stack<pair<int, int> > queued_pixels;
	queued_pixels.push(pair<int, int>(x, y));

	while (queued_pixels.size()) {
		pair<int, int> current_pixel = queued_pixels.top();
		x = current_pixel.first;
		y = current_pixel.second;
		queued_pixels.pop();

		here = pixels + x * 4 + y * stride;
		PRC(here) = newclr;

		if (here + 4 <= maxhere && PRC(here+4) == ownclr)
			queued_pixels.push(pair<int, int>(x + 1, y));
		if (pixels <= here - 4 && PRC(here-4) == ownclr)
			queued_pixels.push(pair<int, int>(x - 1, y));
		if (here + stride <= maxhere && PRC(here+stride) == ownclr)
			queued_pixels.push(pair<int, int>(x, y + 1));
		if (pixels <= here - stride && PRC(here-stride) == ownclr)
			queued_pixels.push(pair<int, int>(x, y - 1));
		if (here + 4 + stride <= maxhere && PRC(here+4+stride) == ownclr)
			queued_pixels.push(pair<int, int>(x + 1, y + 1));
		if (here - 4 + stride <= maxhere && PRC(here-4+stride) == ownclr)
			queued_pixels.push(pair<int, int>(x - 1, y + 1));
		if (pixels <= here + 4 - stride && PRC(here+4-stride) == ownclr)
			queued_pixels.push(pair<int, int>(x + 1, y - 1));
		if (pixels <= here - 4 - stride && PRC(here-4-stride) == ownclr)
			queued_pixels.push(pair<int, int>(x - 1, y - 1));
	}

	cairo_surface->mark_dirty();
}

/******************************************************************************/
/*
 \brief		starting from a pixel at xy within a "component" aka a blob of
 same-colored pixels, increase x until it is next to a new color
 */
/******************************************************************************/
void Surface::run_to_border(int& x, int& y) {
	guint8* pixels = cairo_surface->get_data();
	int stride = cairo_surface->get_stride();

	guint32 start_color = PRC(pixels + x*4 + y * stride);

	if (start_color == 0) {
		PRC(pixels + x*4 + y * stride) = RED;
		save_debug_image("error_runtoborder");
		std::stringstream msg;
		msg << "run_to_border: start_color == 0 at (" << x << "," << y << ")\n";
		throw std::logic_error(msg.str());
	}

	while (PRC(pixels + x*4 + y*stride) == start_color)
		x++;
}

int offset8[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1,
		-1 }, { 0, -1 }, { 1, -1 } };
int offset4[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

/******************************************************************************/
/*
 \brief
 \retval	true if free for growing components
 */
/******************************************************************************/
inline bool Surface::allow_grow(int x, int y, guint32 ownclr) {
	if (x <= 0 || y <= 0)
		return false;
	if (x >= cairo_surface->get_width() - 1)
		return false;
	if (y >= cairo_surface->get_height() - 1)
		return false;

	guint8* pixels = cairo_surface->get_data();
	int stride = cairo_surface->get_stride();

	for (int i = 7; i >= 0; i--) {
		int cx = x + offset8[i][0];
		int cy = y + offset8[i][1];

		guint8* pixel = pixels + cx * 4 + cy * stride;

		// surrounding pixel != own color, not black -> other component!
		if (PRC(pixel) != ownclr && (PRC(pixel) | OPAQUE) != BLACK)
			return false;
	}

	return true;
}

int growoff_o[3][3][2] = { { { 0, -1 }, { -1, -1 }, { -1, 0 } }, { { 1, -1 }, {
		0, 0 }, { -1, 1 } }, { { 1, 0 }, { 1, 1 }, { 0, 1 } } };

int growoff_i[3][3][2] = { { { -1, 0 }, { -1, 1 }, { 0, 1 } }, { { -1, -1 }, {
		0, 0 }, { 1, 1 } }, { { 0, -1 }, { 1, -1 }, { 1, 0 } } };

/******************************************************************************/
/*
 */
/******************************************************************************/
void Surface::calculate_outline(const int x, const int y,
		vector<pair<int, int> >& outside, vector<pair<int, int> >& inside) {
	guint8* pixels = cairo_surface->get_data();
	int stride = cairo_surface->get_stride();
	int max_y = cairo_surface->get_height();

	guint32 owncolor = PRC(pixels + x*4 + y*stride);

	int xstart = x;
	int ystart = y;

	run_to_border(xstart, ystart); //change xstart so that xstart++ would be outside of the component
	int xout = xstart;
	int yout = ystart;
	int xin = xout - 1;
	int yin = yout;

	outside.push_back(pair<int, int>(xout, yout));

	int blasts = 0;

	while (true) {
		int i;
		int steps = 0; // number of steps done in 1 iteration of the while loop

		// step outside
		for (i = 0; i < 8; i++) {
			int xoff = xout - xin + 1;
			int yoff = yout - yin + 1;
			int xnext = xin + growoff_o[xoff][yoff][0];
			int ynext = yin + growoff_o[xoff][yoff][1];

			if (xnext == xstart && ynext == ystart) {
				outside.push_back(pair<int, int>(xout, yout));
				outside.push_back(pair<int, int>(xstart, ystart));
				return;
			}

			if (xnext < 0 || ynext < 0 || xnext > stride || ynext > max_y) {
				save_debug_image("error_outerpath");
				std::stringstream msg;
				msg << "Outside path reaches image margins at " << xin << ","
						<< yin << ")\n";
				throw std::logic_error(msg.str());
			}

			guint8* next = pixels + xnext * 4 + ynext * stride;

			if (PRC(next) != owncolor) {
				outside.push_back(pair<int, int>(xout, yout));
				xout = xnext;
				yout = ynext;
			} else
				break;
		}
		if (i == 8) {
			save_debug_image("error_outsideoverstepping");
			std::stringstream msg;
			msg << "Outside over-stepping at in(" << xin << "," << yin << ")\n";
			throw std::logic_error(msg.str());
		}

		steps += i;

		// step inside
		for (i = 0; i < 8; i++) {
			int xoff = xin - xout + 1;
			int yoff = yin - yout + 1;
			int xnext = xout + growoff_i[xoff][yoff][0];
			int ynext = yout + growoff_i[xoff][yoff][1];
			// next pixel  is checked clockwise
			guint8* next = pixels + xnext * 4 + ynext * stride;
			if (xnext < 0 || ynext < 0 || xnext > stride || ynext > max_y) {
				save_debug_image("error_innerpath");
				std::stringstream msg;
				msg << "Inside path reaches image margins at " << xin << ","
						<< yin << ")\n";
				throw std::logic_error(msg.str());
			}

			if (PRC(next) == owncolor) {
				inside.push_back(pair<int, int>(xin, yin));
				xin = xnext;
				yin = ynext;
			} else
				break;
		}
		if (i == 8) {
			save_debug_image("error_insideoverstepping");
			std::stringstream msg;
			msg << "Inside over-stepping at out(" << xout << "," << yout
					<< ")\n";
			throw std::logic_error(msg.str());
		}

		steps += i;

		// check whether we made any progress calculating the trace outline.
		// if we haven't, our algorithm is deadlocked by stray pixels
		// we try to resolve this by enforcing the algorithm's constraints
		// for the components
		if (steps == 0) {
			int changes = 0;
			// test constraints for surrounding pixels, enforce if necessary
			for (i = 0; i < 8; i++) {
				int cx = xin + offset8[i][0];
				int cy = yin + offset8[i][1];
				guint8* pixel = pixels + cx * 4 + cy * stride;

				if (allow_grow(cx, cy, owncolor)) {
					PRC(pixel) = owncolor;
					changes++;
				}

				// if a component pixel can't be reached non-diagonally, clear it
				// even if it was set just now
				int j;
				for (j = 0; j < 4; j++) {
					if (PRC( pixels + (cx + offset4[j][0]) * 4
							+ (cy + offset4[j][1]) * stride ) != BLACK)
						break;
				}
				if (j == 4) {
					PRC(pixel) = BLACK;
					changes++;
				}
			}
			if (allow_grow(xstart, ystart, owncolor))
				PRC(pixels+xstart*4+ystart*stride) = owncolor;

			if (changes == 0) {
				PRC(pixels + xin*4 + yin*stride) |= RED;
				PRC(pixels + xout*4 + yout*stride) |= BLUE;
				save_debug_image("failed_repair");
				std::stringstream msg;
				msg << "Failed repairing @ (" << xin << "," << yin << ")\n";
				throw std::logic_error(msg.str());
			} else
				blasts++;

			// start right at the beginning. still more efficient than keeping
			// the history necessary to be able to continue next to the problem.
			inside.clear();
			outside.clear();
			xstart = x;
			ystart = y;
			run_to_border(xstart, ystart);
			xout = xstart;
			yout = ystart;
			xin = xout - 1;
			yin = yout;
			outside.push_back(pair<int, int>(xout, yout));
			continue;
		}
	}

	fprintf(stderr, "blasts: %d", blasts);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
guint Surface::grow_a_component(int x, int y, int& contentions) {
	if (x < 0 || x >= cairo_surface->get_width() || y < 0
			|| y >= cairo_surface->get_height()) {
		std::stringstream msg;
		msg << "grow_a_component(): invalid starting point: (" << x << "," << y
				<< ")";
		throw std::logic_error(msg.str());
	}

	contentions = 0;

	vector<pair<int, int> > outside, inside;
	calculate_outline(x, y, outside, inside);

	guint8* pixels = cairo_surface->get_data();
	int stride = cairo_surface->get_stride();

	uint pixels_changed = 0;

	guint32 ownclr = PRC(pixels + x*4 + y*stride);

	for (uint i = 0; i < outside.size(); i++) {
		pair<int, int> coord = outside[i];

		if (allow_grow(coord.first, coord.second, ownclr)) {
			PRC(pixels + coord.first*4 + coord.second*stride) = ownclr;
			pixels_changed++;
		} else {
			contentions++;
		}
	}

	return pixels_changed;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Surface::add_mask(shared_ptr<Surface> mask_surface) {
	Cairo::RefPtr<Cairo::ImageSurface> mask_cairo_surface =
			mask_surface->cairo_surface;

	int max_x = cairo_surface->get_width();
	int max_y = cairo_surface->get_height();
	int stride = cairo_surface->get_stride();

	if (max_x != mask_cairo_surface->get_width()
			|| max_y != mask_cairo_surface->get_height()
			|| stride != mask_cairo_surface->get_stride()) {
		throw std::logic_error("Surface shapes don't match.");
	}

	guint8* pixels = cairo_surface->get_data();
	guint8* mask_pixels = mask_cairo_surface->get_data();

	for (int y = 0; y < max_y; y++) {
		for (int x = 0; x < max_x; x++) {
			PRC(pixels + x*4 + y*stride) &=
					PRC(mask_pixels + x*4 + y*stride); /* engrave only on the surface area */
			PRC(pixels + x*4 + y*stride) |=
					(~PRC(mask_pixels + x*4 + y*stride) & (RED | BLUE)); /* tint the outiside in an own color to block extension */
		}
	}
}

#include <boost/format.hpp>

/******************************************************************************/
/*
 */
/******************************************************************************/
void Surface::save_debug_image(string message) {
	static uint debug_image_index = 0;

	opacify(pixbuf);
	pixbuf->save(outputdir +
			(boost::format("outp%1%_%2%.png") % debug_image_index % message).str(),
			"png");
	debug_image_index++;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Surface::opacify(Glib::RefPtr<Gdk::Pixbuf> pixbuf) {
	int stride = pixbuf->get_rowstride();
	guint8* pixels = pixbuf->get_pixels();
	for (int y = 0; y < pixbuf->get_height(); y++) {
		for (int x = 0; x < pixbuf->get_width(); x++) {
			PRC(pixels + x*4 + y*stride) |= OPAQUE;
		}
	}
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Surface::fill_outline(double linewidth) {
	/* paint everything white that can not be reached from outside the image */

	int stride = pixbuf->get_rowstride();
	guint8* pixels = pixbuf->get_pixels();

	/* in order to find out what is "outside", we need to walk "around' the image */
	for (int x = 0; x < pixbuf->get_width(); x++) {
		if (PRC(pixels + x*4 + 0*stride) != BLACK)
			throw std::logic_error("Non-black pixel at top border");
		if (PRC(pixels + x*4 + (pixbuf->get_height() - 1)*stride) != BLACK)
			throw std::logic_error("Non-black pixel at bottom border");
	}
	for (int y = 0; y < pixbuf->get_height(); y++) {
		if (PRC(pixels + 0*4 + y*stride) != BLACK)
			throw std::logic_error("Non-black pixel at left border");
		if (PRC(pixels + (pixbuf->get_width() - 1)*4 + y*stride) != BLACK)
			throw std::logic_error("Non-black pixel at right border");
	}

	fill_a_component(0, 0, BLUE);

	/* everything else (that is, the area of the board) will be black
	 *
	 * saving the line where black starts for later when we need something
	 * black so grow's run_to_border can work.
	 */
	int first_line_with_black = 0;
	for (int y = 0; y < pixbuf->get_height(); y++) {
		for (int x = 0; x < pixbuf->get_width(); x++) {
			if (PRC(pixels + x*4 + y*stride) != BLUE) {
				PRC(pixels + x*4 + y*stride) = BLACK;
				if (first_line_with_black == 0) {
					first_line_with_black = y;
				}
			}
		}
	}

	/* compensate for growth induced by line thicknesses.
	 *
	 * this could be done by growing the outline by a reduced amount later
	 * (providing the lines are not wider than the tool), but by doing the
	 * reduction now, the lines are already compensated for in the masking
	 * step. thus, the engraving bit will really engrave once around the
	 * outline instead of engraving in an area that is going to be removed,
	 * potentially creating neater edges and providing a more realistic
	 * rendition in png and gcode previews.
	 */
	int grow = linewidth / 2 * dpi;
	int contentions = 0;
	int added = 0;
	for (int i = 0; i < grow; ++i) {
		// starting at the very left 
		added = grow_a_component(0, first_line_with_black + grow, contentions);
	}
	// if you can think of a sane situation in which either of this could
	// occur and nevertheless give a meaningful result, change it to a
	// warning.
	if (!added)
		throw std::logic_error(
				"Shrinking the outline by half the line width came to a halt.");
	if (contentions)
		throw std::logic_error(
				"Shrinking the outline collided with something while there should not be anything.");

	for (int y = 0; y < pixbuf->get_height(); y++) {
		for (int x = 0; x < pixbuf->get_width(); x++) {
			if (PRC(pixels + x*4 + y*stride) == BLUE)
				PRC(pixels + x*4 + y*stride) = BLACK;
			else
				PRC(pixels + x*4 + y*stride) = WHITE;
		}
	}

	save_debug_image("outline_filled");
}
