/*
 *  svg_exporter.cpp
 *  
 *	svg exporter for milling paths and drill-holes.
 *
 *  Created by Ingo Randolf
 *
 *	you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *  
 *
 */

#ifndef SVGEXPORTER_H
#define SVGEXPORTER_H


#include <map>
using std::map;

#include <string>
using std::string;

#include <vector>
using std::vector;
#include <map>
using std::map;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

extern "C" {
#include <gerbv.h>
}


#include <glibmm/refptr.h>
#include <gdkmm/pixbuf.h>
#include <glibmm/ustring.h>
using Glib::ustring;

#include "coord.hpp"
#include "exporter.hpp"


class SVG_Exporter
{
public:
	//SVG_Exporter(string filename, const ivalue_t board_width, const ivalue_t board_height);
	SVG_Exporter( shared_ptr<Board> board );
	~SVG_Exporter();
	
	void create_svg( string filename );
	void set_rand_color();
	void move_to(ivalue_t x, ivalue_t y);
	void line_to(ivalue_t x, ivalue_t y);
	void circle(ivalue_t x, ivalue_t y, ivalue_t rad);
	void close_path();
	void stroke();
	void show_page();
	void copy_page();
	

protected:
	
	int dpi;
	
	shared_ptr<Board> board;
	
	Cairo::RefPtr<Cairo::SvgSurface> cairo_svgsurface;
	Cairo::RefPtr<Cairo::Context> cr;

};

#endif // SVGEXPORTER_H
