
#ifndef LAYER_H
#define LAYER_H

#include <string>
using std::string;
#include <vector>
using std::vector;

#include <boost/noncopyable.hpp>

#include "coord.hpp"
#include "surface.hpp"
#include "mill.hpp"

class Layer : boost::noncopyable
{
public:
	Layer( const string& name, shared_ptr<Surface> surface, shared_ptr<RoutingMill> manufacturer, bool backside );
	
	vector< shared_ptr<icoords> > get_toolpaths();
	shared_ptr<RoutingMill> get_manufacturer();
	string get_name() { return name; };

private:
	string name;
	bool mirrored;
	shared_ptr<Surface> surface;
	shared_ptr<RoutingMill>    manufacturer;

	friend class Board;
};

#endif // LAYER_H
