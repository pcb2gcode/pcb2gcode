/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2011 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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

#include <memory>
using std::shared_ptr;

extern "C" {
#include <gerbv.h>
}

#include <glibmm/refptr.h>
#include <gdkmm/pixbuf.h>
#include <glibmm/ustring.h>
using Glib::ustring;

#include "geometry.hpp"
#include "exporter.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class SVG_Exporter
{
public:
    //SVG_Exporter(string filename, const ivalue_t board_width, const ivalue_t board_height);
    SVG_Exporter(shared_ptr<Board> board);
    ~SVG_Exporter();

    void create_svg(string filename);
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
