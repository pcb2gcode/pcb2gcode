#ifndef EXPORTER_H
#define EXPORTER_H

#include <vector>
using std::vector;

#include <string>
using std::string;
using std::pair;

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include "coord.hpp"
#include "board.hpp"

class Exporter : public boost::noncopyable
{
public:
	Exporter( shared_ptr<Board> board ) {};

	virtual void export_all() = 0;
};

#endif // EXPORTER_H
