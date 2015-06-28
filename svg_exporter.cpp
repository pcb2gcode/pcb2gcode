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

#include <iostream>
using std::cout;
using std::endl;

#include <fstream>
#include <iomanip>
using namespace std;

#include "svg_exporter.hpp"

#include <cstring>

using std::pair;

/******************************************************************************/
/*
 */
/******************************************************************************/
SVG_Exporter::SVG_Exporter(shared_ptr<Board> board)
{
    this->dpi = 72;
    this->board = board;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
SVG_Exporter::~SVG_Exporter()
{
    // something to do here?
    // closing svg??
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::create_svg(string filename)
{

    //create the svg
    cairo_svgsurface = Cairo::SvgSurface::create(filename,
                       board->get_width() * dpi, board->get_height() * dpi);
    //get the cairo drawing area
    cr = Cairo::Context::create(cairo_svgsurface);

    //setup
    cr->set_line_width(0.1); // set line width
    cr->set_source_rgba(1.0, 0.0, 0.0, 1.0); // set initial color...
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::set_rand_color()
{
    cr->set_source_rgb((rand() % 256) / 256., (rand() % 256) / 256.,
                       (rand() % 256) / 256.);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::move_to(ivalue_t x, ivalue_t y)
{
    cr->move_to(x * dpi, y * dpi);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::line_to(ivalue_t x, ivalue_t y)
{
    cr->line_to(x * dpi, y * dpi);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::circle(ivalue_t x, ivalue_t y, ivalue_t rad)
{
    cr->arc(x * dpi, y * dpi, rad, 0.0, 2.0 * M_PI);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::close_path()
{
    cr->close_path();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::stroke()
{
    cr->stroke();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::show_page()
{
    cr->show_page();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void SVG_Exporter::copy_page()
{
    cr->copy_page();
}
