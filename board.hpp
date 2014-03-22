/*!\defgroup BOARD*/
/******************************************************************************/
/*!
 \file       board.hpp
 \brief

 \version
 04.08.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Formatted the code with the Eclipse code styler (Style: K&R).
 - Prepared commenting the code

 \version
 1.1.4 - 2009, 2010, Patrick Birnzain <pbirnzain@users.sourceforge.net> and others

 \copyright
 pcb2gcode is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 pcb2gcode is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.

 \ingroup    BOARD
 */
/******************************************************************************/

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#include <stdexcept>
#include <sstream>

#include <iostream>
using std::cout;
using std::endl;

#include <map>
using std::map;
#include <vector>
using std::vector;
using std::pair;

#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#include <boost/tuple/tuple.hpp>
using boost::tuple;

#include "coord.hpp"
#include "surface.hpp"
#include "layer.hpp"

#include "mill.hpp"

/******************************************************************************/
/*
 \brief	Represents a printed circuit board.

 This class calculates the required minimum board size
 and applies the (not yet) described operations on the
 photoplots of each layer to calculate the toolpaths.
 */
/******************************************************************************/
class Board {
public:
	Board(int dpi, bool fill_outline, double outline_width);

	void prepareLayer(string layername, shared_ptr<LayerImporter> importer,
			shared_ptr<RoutingMill> manufacturer, bool topside,
			bool mirror_absolute);
	void set_margins(double margins) { margin = margins;	};
	ivalue_t get_width();
	ivalue_t get_height();
	ivalue_t get_min_x() {	return min_x; };
	ivalue_t get_max_x() {	return max_x; };
	ivalue_t get_min_y() {	return min_y; };
	ivalue_t get_max_y() {	return max_y; };

	vector<string> list_layers();
	shared_ptr<Layer> get_layer(string layername);
	vector<shared_ptr<icoords> > get_toolpath(string layername);

	void createLayers(); // should be private
	uint get_dpi();

private:
	ivalue_t margin;
	uint dpi;
	bool fill_outline;
	double outline_width;
	ivalue_t min_x;
	ivalue_t max_x;
	ivalue_t min_y;
	ivalue_t max_y;

	/* The Layer gets constructed from data prepared in
	 * prepareLayer after the size calculations are done in createLayers.
	 * (It can't be constructed in prepareLayer as the surface which gets
	 * passed to the Layer at construction time needs the sizes to be
	 * created.)
	 * In the meantime, the construction arguments get carried around in
	 * prep_t tuples, whose signature must basically match the construction
	 * signature of Layer.
	 */
	typedef tuple<shared_ptr<LayerImporter>, shared_ptr<RoutingMill>, bool, bool> prep_t;
	map<string, prep_t> prepared_layers;
	map<string, shared_ptr<Layer> > layers;
};

#endif // BOARD_H
