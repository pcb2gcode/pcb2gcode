#ifndef NGCEXPORTER_H
#define NGCEXPORTER_H

#include <vector>
using std::vector;

#include <string>
using std::string;
using std::pair;

#include <fstream>
using std::ofstream;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include "coord.hpp"
#include "mill.hpp"
#include "exporter.hpp"

class NGC_Exporter : public Exporter
{
public:
	NGC_Exporter( shared_ptr<Board> board );

	/* virtual void add_path( shared_ptr<icoords> ); */
        /* virtual void add_path( vector< shared_ptr<icoords> > ); */

	void add_header( string );
	void export_all();

protected:
	void export_layer( shared_ptr<Layer> layer );

	shared_ptr<Board> board;
	vector<string> header;
};

#endif // NGCEXPORTER_H
