#include "surface.hpp"
using std::pair;

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
Surface::get_toolpath( shared_ptr<RoutingMill> mill, bool mirrored )
{
        coords components = fill_all_components();

        int added = -1;
        int contentions = 0;
        int grow = mill->tool_diameter / 2 * dpi;

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
		if( mirrored ) {
			BOOST_FOREACH( coordpair c, outside ) {
				outline->push_back( icoordpair(
							    // tricky calculations
							    // (cairo_surface->get_width() - c.first - zero_x)/(double)dpi,
							    min_x + max_x - xpt2i( c.first ),
							    min_y + max_y - ypt2i(c.second) ) );
			}
		} else {
			BOOST_FOREACH( coordpair c, outside ) {
				outline->push_back( icoordpair(
							    // tricky calculations
							    xpt2i(c.first),
							    min_y + max_y - ypt2i(c.second) ) );
			}
		}

		outside.clear();
                toolpath.push_back(outline);
        }

        save_debug_image();
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
                        if( (PRC(pixels + x*4 + y*stride) | 0xFF000000) == 0xFFFFFFFF )
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
                if( PRC(pixel) != ownclr && ( PRC(pixel) & 0x00FFFFFF) != 0x00000000 )
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

void Surface::calculate_outline(int x, int y,
                                vector< pair<int,int> >& outside, vector< pair<int,int> >& inside)
{
        guint8* pixels = cairo_surface->get_data();
        int stride = cairo_surface->get_stride();

        guint32 owncolor = PRC(pixels + x*4 + y*stride);

        run_to_border(x,y);
        int xout = x;
        int yout = y;
        int xin = x-1;
        int yin = y;

        outside.push_back( pair<int,int>(xout, yout) );

        int xstart = xout;
        int ystart = yout;

        while(true)
        {
                // step outside
                for(int i = 0; i < 8; i++)
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

                // step inside
                for(int i = 0; i < 8; i++)
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

#include <boost/format.hpp>

void Surface::save_debug_image()
{
        static uint debug_image_index = 0;

        opacify(pixbuf);
        pixbuf->save( (boost::format("outp%1%.png") % debug_image_index).str() , "png");
	debug_image_index++;
}

void Surface::opacify( Glib::RefPtr<Gdk::Pixbuf> pixbuf )
{
        for(int y = 0; y < pixbuf->get_height(); y++ )
        {
                guint8* pixel = pixbuf->get_pixels() + 3 + y * pixbuf->get_rowstride();

                for(int x = 0; x < pixbuf->get_width(); x++ )
                {
                        PRC(pixel) |= 0x000000FF;
                        pixel += 4;
                }
        }
}

