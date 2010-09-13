#ifndef BOARD_H
#define BOARD_H

#include <glibmm/refptr.h>
#include <cairomm/cairomm.h>
#include <gdk/gdkcairo.h>

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

//! Represents a printed circuit board.
/*! This class calculates the required minimum board size
 *  and applies the (not yet) described operations on the
 *  photoplots of each layer to calculate the toolpaths.
 */
class Board
{
public:
	Board();

	void prepareLayer( string layername, shared_ptr<LayerImporter> importer, shared_ptr<RoutingMill> manufacturer, bool topside );
	void set_margins( double margins ) { margin = margins; };

	ivalue_t get_width();
	ivalue_t get_height();
	
	ivalue_t get_min_x() { return min_x; };
	ivalue_t get_max_x() { return max_x; };
	ivalue_t get_min_y() { return min_y; };
	ivalue_t get_max_y() { return max_y; };	

	vector< string > list_layers();
	shared_ptr<Layer> get_layer( string layername );
	vector< shared_ptr<icoords> > get_toolpath( string layername );

	void createLayers();	// should be private

private:
	ivalue_t margin;
	uint dpi;
	ivalue_t min_x;
	ivalue_t max_x;
	ivalue_t min_y;
	ivalue_t max_y;

	typedef tuple< shared_ptr<LayerImporter>, shared_ptr<RoutingMill>, bool > prep_t;
	map< string, prep_t > prepared_layers;
	map< string, shared_ptr<Layer> >    layers;
};

#endif // BOARD_H

