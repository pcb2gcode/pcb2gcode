#ifndef MILL_H
#define MILL_H

#include <stdint.h>

class Mill
{
public:
	virtual ~Mill() {};

	double feed;
	int    speed;

	double zchange;
	double zsafe;
	double zwork;
};

class RoutingMill : public Mill
{
public:
	double tool_diameter;
};

class Isolator : public RoutingMill
{
};

class Cutter : public RoutingMill
{
public:
	bool do_steps;
	double stepsize;
};

class Driller : public Mill
{
};

#endif // MILL_H
