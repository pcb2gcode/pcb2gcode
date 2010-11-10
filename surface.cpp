
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

#include "surface.hpp"
using std::pair;

// color definitions for the ARGB32 format used

#define OPAQUE 0xFF000000
#define RED 0xFFFF0000
#define GREEN 0xFF00FF00
#define BLUE 0xFF0000FF
#define WHITE ( RED | GREEN | BLUE )
// while equal by value, OPAQUE is used for |-ing and BLACK for setting or comparison
#define BLACK ( RED & GREEN & BLUE )

void Surface::make_the_surface(uint width, uint height)
{
        pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB,
                                     true, 8, width, height);

        cairo_surface = Cairo::ImageSurface::create(pixbuf->get_pixels(),
                                                    Cairo::FORMAT_ARGB32, width, height, pixbuf->get_rowstride());
}

#include <boost/foreach.hpp>

#include <iostream>
using std::cerr;
using std::endl;

#define PRC(x) *(reinterpret_cast<guint32*>(x))

Surface::Surface( guint dpi, ivalue_t min_x, ivalue_t max_x, ivalue_t min_y, ivalue_t max_y )
        : dpi(dpi), min_x(min_x), max_x(max_x), min_y(min_y), max_y(max_y),
	  zero_x(-min_x*(ivalue_t)dpi + (ivalue_t)procmargin), zero_y(-min_y*(ivalue_t)dpi + (ivalue_t)procmargin), clr(32)
{
        make_the_surface( (max_x - min_x) * dpi + 2*procmargin, (max_y - min_y) * dpi + 2*procmargin );
}

void Surface::render( boost::shared_ptr<LayerImporter> importer ) throw(import_exception)
{
        importer->render(cairo_surface, dpi, min_x - static_cast<ivalue_t>(procmargin)/dpi,
			 min_y - static_cast<ivalue_t>(procmargin)/dpi );
}

#include <iostream>
using std::cout;
using std::endl;

vector< shared_ptr<icoords> >
Surface::get_toolpath( shared_ptr<RoutingMill> mill, bool mirrored, bool mirror_absolute )
{
        coords components = fill_all_components();

        int added = -1;
        int contentions = 0;
        int grow = mill->tool_diameter / 2 * dpi;
	ivalue_t double_mirror_axis = mirror_absolute ? 0 : (min_x + max_x);

        for(int i = 0; i < grow && added != 0; i++)
        {
                added = 0;

                BOOST_FOREACH( coordpair c, components ) {
                        added += grow_a_component(c.first, c.second, contentions);
                }
        }

        if(contentions) {
                cerr << "Warning: pcb2gcode hasn't been able to fulfill all"
                     << " clearance requirements and tried a best effort approach"
                     << " instead. You may want to check the g-code output and"
                     << " possibly use a smaller milling width.\n";
        }

        vector< shared_ptr<icoords> > toolpath;
        coords inside, outside;
        
        BOOST_FOREACH( coordpair c, components ) {
                calculate_outline( c.first, c.second, outside, inside );
                inside.clear();

                shared_ptr<icoords> outline( new icoords() );

		// i'm not sure wheter this is the right place to do this...
		// that "mirrored" flag probably is a bad idea.
		BOOST_FOREACH( coordpair c, outside ) {
			outline->push_back( icoordpair(
						    // tricky calculations
						    mirrored ? (double_mirror_axis - xpt2i(c.first)) : xpt2i(c.first),
						    min_y + max_y - ypt2i(c.second) ) );
		}

		outside.clear();
                toolpath.push_back(outline);
        }

        save_debug_image("traced");
        return toolpath;
}

guint32 Surface::get_an_unused_color()
{
        /// @todo this is quite crappy...

        // clr = ((rand()%256) << 24) + ((rand()%256) << 16) + ((rand()%256) << 8) + (rand()%256);
        clr = rand();

        return clr;
}

std::vector< std::pair<int,int> > Surface::fill_all_components()
{
        std::vector< pair<int,int> > components;
        int max_x = cairo_surface->get_width() - 1;
        int max_y = cairo_surface->get_height() - 1;
        guint8* pixels = cairo_surface->get_data();
        int stride = cairo_surface->get_stride();

        for(int y = 5; y <= max_y; y += 10)
        {
                for(int x = 5; x <= max_x; x += 10)
                {
                        if( (PRC(pixels + x*4 + y*stride) | OPAQUE) == WHITE )
                        {
                                components.push_back( pair<int,int>(x,y) );
                                fill_a_component(x, y, get_an_unused_color());
                        }
                }
        }

        return components;
}

#include <stack>

// fill_a_component does not do any image boundary checks out of performance reasons.
void Surface::fill_a_component(int x, int y, guint32 argb)
{
        guint32 newclr = argb;

        guint8* pixels = cairo_surface->get_data();
        int stride = cairo_surface->get_stride();

        guint8* here = pixels + x*4 + y*stride;

        guint32 ownclr = PRC(here);

        std::stack< pair<int, int> > queued_pixels;
        queued_pixels.push( pair<int, int>(x,y) );

        while( queued_pixels.size() )
        {
                pair<int,int> current_pixel = queued_pixels.top();
                x = current_pixel.first;
                y = current_pixel.second;
                queued_pixels.pop();

                here = pixels + x*4 + y*stride;
                PRC(here) = newclr;

                if( PRC(here+4) == ownclr )
                        queued_pixels.push( pair<int,int>(x+1, y) );
                if( PRC(here-4) == ownclr )
                        queued_pixels.push( pair<int,int>(x-1, y) );
                if( PRC(here+stride) == ownclr )
                        queued_pixels.push( pair<int,int>(x, y+1) );
                if( PRC(here-stride) == ownclr )
                        queued_pixels.push( pair<int,int>(x, y-1) );
        }

        cairo_surface->mark_dirty();
}

void Surface::run_to_border(int& x, int& y)
{
        guint8* pixels = cairo_surface->get_data();
        int stride = cairo_surface->get_stride();

        guint32 start_color = PRC(pixels + x*4 + y * stride);

	if( start_color == 0 ) {
		PRC(pixels + x*4 + y * stride) = RED;
		save_debug_image("error_runtoborder");
		std::stringstream msg;
		msg << "run_to_border: start_color == 0 at ("
		    << x << "," << y << ")\n";
		throw std::logic_error( msg.str() );
	}

        while( PRC(pixels + x*4 + y*stride) == start_color )
                x++;
}

int offset8[8][2] = {{1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}};

// true if free for growing components
inline bool Surface::allow_grow(int x, int y, guint32 ownclr)
{
        if(x <= 0 || y <= 0)
                return false;
        if(x >= cairo_surface->get_width()-1)
                return false;
        if(y >= cairo_surface->get_height()-1)
                return false;

        guint8* pixels = cairo_surface->get_data();
        int stride = cairo_surface->get_stride();

        for(int i = 7; i >= 0; i--)
        {
                int cx = x + offset8[i][0];
                int cy = y + offset8[i][1];

                guint8* pixel = pixels + cx * 4 + cy * stride;

                // surrounding pixel != own color, not black -> other component!
                if( PRC(pixel) != ownclr && ( PRC(pixel) | OPAQUE) != BLACK )
                        return false;
        }

        return true;
}

int growoff_o[3][3][2] =
{
        {{ 0,-1}, {-1,-1}, {-1,0}},
        {{ 1,-1}, { 0, 0}, {-1,1}},
        {{ 1, 0}, { 1, 1}, { 0,1}}
};

int growoff_i[3][3][2] =
{
        {{-1, 0}, {-1, 1}, {0, 1}},
        {{-1,-1}, { 0, 0}, {1, 1}},
        {{ 0,-1}, { 1,-1}, {1, 0}}
};

void Surface::calculate_outline(const int x, const int y,
                                vector< pair<int,int> >& outside, vector< pair<int,int> >& inside)
{
        guint8* pixels = cairo_surface->get_data();
        int stride = cairo_surface->get_stride();

        guint32 owncolor = PRC(pixels + x*4 + y*stride);

        int xstart = x;
        int ystart = y;

        run_to_border(xstart,ystart);
        int xout = xstart;
        int yout = ystart;
        int xin = xout-1;
        int yin = yout;

        outside.push_back( pair<int,int>(xout, yout) );

        while(true)
        {
		int i;

                // step outside
                for(i = 0; i < 8; i++)
                {
                        int xoff = xout - xin + 1;
                        int yoff = yout - yin + 1;
                        int xnext = xin + growoff_o[xoff][yoff][0];
                        int ynext = yin + growoff_o[xoff][yoff][1];

                        if(xnext == xstart && ynext == ystart)
                        {
                                outside.push_back( pair<int,int>(xout, yout) );
                                return;
                        }

                        guint8* next = pixels + xnext*4 + ynext*stride;

                        if( PRC(next) != owncolor )
                        {
                                outside.push_back( pair<int,int>(xout, yout) );
                                xout = xnext;
                                yout = ynext;
                        }
                        else
                                break;
                }

		// check whether stepping was successful.
		// this prevents endless loops that can occur in rare cases
		if( i == 0 && xout != xstart ) {
			// blast the problem
			for(; i < 8; i++) {
				int cx = xout + offset8[i][0];
				int cy = yout + offset8[i][1];

				guint8* pixel = pixels + cx * 4 + cy * stride;
				PRC(pixel) = 0;
			}
			PRC(pixels+xstart*4+ystart*stride) = owncolor;
			// start right at the beginning. still more efficient than keeping
			// the history necessary to be able to continue next to the problem.
			inside.clear();
			outside.clear();
			xstart = x;
			ystart = y;
			run_to_border(xstart,ystart);
			xout = xstart;
			yout = ystart;
			xin = xout-1;
			yin = yout;
			outside.push_back( pair<int,int>(xout, yout) );
			continue;
		} else if( i == 8 ) {
			save_debug_image("error_outsideoverstepping");
			std::stringstream msg;
			msg << "Outside over-stepping at in(" << xin << "," << yin << ")\n";
			throw std::logic_error( msg.str() );
		}

                // step inside
                for(i = 0; i < 8; i++)
                {
                        int xoff = xin - xout + 1;
                        int yoff = yin - yout + 1;
                        int xnext = xout + growoff_i[xoff][yoff][0];
                        int ynext = yout + growoff_i[xoff][yoff][1];
                        guint8* next = pixels + xnext*4 + ynext*stride;

                        if( PRC(next) == owncolor )
                        {
                                inside.push_back( pair<int,int>(xin, yin) );
                                xin = xnext;
                                yin = ynext;
                        }
                        else
                                break;
                }
		if( i == 8 ) {
			save_debug_image("error_insideoverstepping");
			std::stringstream msg;
			msg << "Inside over-stepping at out(" << xout << "," << yout << ")\n";
			throw std::logic_error( msg.str() );
		}
	}
}

guint Surface::grow_a_component(int x, int y, int& contentions)
{
	contentions = 0;

        vector< pair<int,int> > outside, inside;
        calculate_outline(x, y, outside, inside);

        guint8* pixels = cairo_surface->get_data();
        int stride = cairo_surface->get_stride();

        uint pixels_changed = 0;

        guint32 ownclr = PRC(pixels + x*4 + y*stride);

        for(uint i = 0; i < outside.size(); i++)
        {
                pair<int,int> coord = outside[i];

                if( allow_grow(coord.first, coord.second, ownclr) )
                {
                        PRC(pixels + coord.first*4 + coord.second*stride) = ownclr;
                        pixels_changed++;
                } else {
                        contentions++;
                }
        }

        return pixels_changed;
}

void Surface::add_mask( shared_ptr<Surface> mask_surface) {
	Cairo::RefPtr<Cairo::ImageSurface> mask_cairo_surface = mask_surface->cairo_surface;

        int max_x = cairo_surface->get_width();
        int max_y = cairo_surface->get_height();
        int stride = cairo_surface->get_stride();

	if(
			max_x != mask_cairo_surface->get_width() ||
			max_y != mask_cairo_surface->get_height() ||
			stride != mask_cairo_surface->get_stride()
	  ) {
		throw std::logic_error( "Surface shapes don't match." );
	}

        guint8* pixels = cairo_surface->get_data();
        guint8* mask_pixels = mask_cairo_surface->get_data();

        for(int y = 0; y < max_y; y ++)
        {
                for(int x = 0; x < max_x; x ++)
                {
			PRC(pixels + x*4 + y*stride) &= PRC(mask_pixels + x*4 + y*stride); /* engrave only on the surface area */
			PRC(pixels + x*4 + y*stride) |= (~PRC(mask_pixels + x*4 + y*stride) & (RED | BLUE)); /* tint the outiside in an own color to block extension */
                }
        }
}

#include <boost/format.hpp>

void Surface::save_debug_image(string message)
{
        static uint debug_image_index = 0;

        opacify(pixbuf);
        pixbuf->save( (boost::format("outp%1%_%2%.png") % debug_image_index % message).str() , "png");
	debug_image_index++;
}

void Surface::opacify( Glib::RefPtr<Gdk::Pixbuf> pixbuf )
{
        int stride = pixbuf->get_rowstride();
        guint8* pixels = pixbuf->get_pixels();
        for(int y = 0; y < pixbuf->get_height(); y++ )
        {
                for(int x = 0; x < pixbuf->get_width(); x++ )
                {
                        PRC(pixels + x*4 + y*stride) |= OPAQUE;
                }
        }
}

