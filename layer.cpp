
#include "layer.hpp"

Layer::Layer( const string& name, shared_ptr<Surface> surface, shared_ptr<RoutingMill> manufacturer, bool backside )
{
	this->name = name;
	this->mirrored = backside;
	this->surface = surface;
	this->manufacturer = manufacturer;
}

#include <iostream>
using namespace std;

vector< shared_ptr<icoords> >
Layer::get_toolpaths()
{
	return surface->get_toolpath( manufacturer, mirrored );
}

shared_ptr<RoutingMill>
Layer::get_manufacturer()
{
	return manufacturer;
}
