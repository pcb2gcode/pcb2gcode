
/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
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

#include "drill.hpp"

#include <cstring>
#include <boost/scoped_array.hpp>

#include <boost/foreach.hpp>

using std::pair;

ExcellonProcessor::ExcellonProcessor( string drillfile, const ivalue_t board_width ) : board_width(board_width)
{
	project = gerbv_create_project();

	const char* cfilename = drillfile.c_str();
	boost::scoped_array<char> filename( new char[strlen(cfilename) + 1] );
	strcpy(filename.get(), cfilename);

	gerbv_open_layer_from_filename(project, filename.get());
	if( project->file[0] == NULL)
		throw drill_exception();

	preamble=string("G94     ( Inches per minute feed rate. )\n")+
	   "G20     ( Units == INCHES.             )\n"+
	   "G90     ( Absolute coordinates.        )\n";
	postamble=string("M9 ( Coolant off. )\n")+
		"M2 ( Program end. )\n\n";
}

void
ExcellonProcessor::add_header( string header )
{
        this->header.push_back(header);
}

void
ExcellonProcessor::export_ngc( const string of_name, shared_ptr<Driller> driller, bool mirrored, bool mirror_absolute )
{
	/* will not be implemented until i've checked whether there's a reason to drill the
	   holes from the front side. Maybe there are boards without a back side? SMD-only?
	 */
	g_assert( mirrored == true );

	ivalue_t double_mirror_axis = mirror_absolute ? 0 : board_width;

	// open output file
	std::ofstream of; of.open( of_name.c_str() );

	shared_ptr<const map<int,drillbit> > bits = get_bits();
	shared_ptr<const map<int,icoords> > holes = get_holes();	

	// write header to .ngc file
        BOOST_FOREACH( string s, header )
        {
                of << "( " << s << " )" << endl;
        }
        of << endl;

	of << "( This file uses " << bits->size() << " drill bit sizes. )\n\n";

        of.setf( ios_base::fixed );
        of.precision(5);
	of << setw(7);

	// preamble
	of <<preamble
	   << "S" << left << driller->speed << "  ( RPM spindle speed.           )\n"
	   << endl;

	for( map<int,drillbit>::const_iterator it = bits->begin(); it != bits->end(); it++ ) {
		of << "G00 Z" << driller->zchange << " ( Retract )\n"
		   << "T" << it->first << endl
		   << "M5      ( Spindle stop.                )\n"
		   << "M6      ( Tool change.                 )\n"
		   << "(MSG, CHANGE TOOL BIT: to drill size " << it->second.diameter << " " << it->second.unit << ")\n"
		   << "M0      ( Temporary machine stop.      )\n"
		   << "M3      ( Spindle on clockwise.        )\n"
		   << endl;

		const icoords drill_coords = holes->at(it->first);
		icoords::const_iterator coord_iter = drill_coords.begin();

		of << "G81 R" << driller->zsafe << " Z" << driller->zwork << " F" << driller->feed 
		   << " X" << (double_mirror_axis - coord_iter->first) << " Y" << coord_iter->second << endl;
		++coord_iter;

		while( coord_iter != drill_coords.end() ) {
			of << "X" << (double_mirror_axis - coord_iter->first) << " Y" << coord_iter->second << endl;
			++coord_iter;
		}

		of << "\n\n";
	}

	// retract, end
	of << "G00 Z" << driller->zchange << " ( All done -- retract )\n" << endl;

	of << "M9 ( Coolant off. )\n";
	of << "M2 ( Program end. )\n\n";

	of.close();
}

void ExcellonProcessor::millhole(std::ofstream &of,float x, float y,  shared_ptr<Cutter> cutter,float holediameter)
{
	g_assert(cutter);
	double cutdiameter=cutter->tool_diameter+2 * 0.005;

	if(cutdiameter*1.001>=holediameter)
	{
		of<<"G0 X"<< x<<" Y" << y<< endl;
		//of<<"G1 Z"<<cutter->zwork<<endl;
		//of<<"G0 Z"<<cutter->zsafe<<endl<<endl;
		of<<"G1 Z#50"<<endl;
		of<<"G0 Z#51"<<endl<<endl;
	}
	else
	{
		float millr=(holediameter-cutdiameter)/2.;
		of<<"G0 X"<< x+millr<<" Y" << y<< endl;
		
		double z_step = cutter->stepsize;
		//z_step=0.01;
		double z = cutter->zwork + z_step * abs( int( cutter->zwork / z_step ) );
		if( !cutter->do_steps ) {
			z=cutter->zwork;
			z_step=1; //dummy to exit the loop
		}
		int stepcount=abs( int( cutter->zwork / z_step )) ;
		while( z >= cutter->zwork ) 
		{
			//of<<"G1 Z"<<z<<endl;
			of<<"G1 Z[#50+"<<stepcount<<"*#52]"<<endl;
			of<<"G2 I"<<-millr<<" J0"<<endl;
			z -= z_step;
			stepcount--;
		}
		of<<"G0 Z"<<cutter->zsafe<<endl<<endl;
	}
}

//mill larger holes by using a smaller mill-head
void 
ExcellonProcessor::export_ngc( const string outputname,  shared_ptr<Cutter> target, bool mirrored, bool mirror_absolute )
{

	g_assert( mirrored == true );
	g_assert( mirror_absolute == false );
	cerr << "Currently Drilling "<< endl;

	// open output file
	std::stringstream of_name; of_name << outputname << ".ngc";
	std::ofstream of; of.open( of_name.str().c_str() );

	shared_ptr<const map<int,drillbit> > bits = get_bits();
	shared_ptr<const map<int,icoords> > holes = get_holes();	

	// write header to .ngc file
        BOOST_FOREACH( string s, header )
        {
                of << "( " << s << " )" << endl;
        }
        of << endl;

	//of << "( This file uses " << bits->size() << " drill bit sizes. )\n\n";
	of << "( This file uses a mill head of "<<target->tool_diameter<<" to drill the "<<bits->size() <<"bit sizes. )\n\n";

        of.setf( ios_base::fixed );
        of.precision(5);
	of << setw(7);

	// preamble
	of << preamble
	   << "S" << left << target->speed << "  ( RPM spindle speed.           )\n"
	   << endl;
	of<<"F"<<target->feed<<endl;
	
	of<<"#50="<<target->zwork<<" ; zwork"<<endl;
	of<<"#51="<<target->zsafe<<" ; zsafe"<<endl;
	of<<"#52="<<target->stepsize<<" ; stepsize"<<endl;
	
	
	for( map<int,drillbit>::const_iterator it = bits->begin(); it != bits->end(); it++ ) {
		
		float diameter=it->second.diameter;
		//cerr<<"bit:"<<diameter<<endl;
		const icoords drill_coords = holes->at(it->first);
		icoords::const_iterator coord_iter = drill_coords.begin();
		
		millhole(of,  board_width - coord_iter->first, coord_iter->second, target,diameter);
		++coord_iter;

		while( coord_iter != drill_coords.end() ) {

			millhole(of,  board_width - coord_iter->first, coord_iter->second, target,diameter);
			++coord_iter;
		}
	
	}

	// retract, end
	of << "G00 Z" << target->zchange << " ( All done -- retract )\n" << endl;

	of << postamble;

	of.close();
}

void
ExcellonProcessor::parse_bits()
{
	bits = shared_ptr< map<int,drillbit> >( new map<int,drillbit>() );

	for( gerbv_drill_list_t* currentDrill = project->file[0]->image->drill_stats->drill_list; currentDrill;
	     currentDrill = currentDrill->next ) {
		drillbit curBit;
		curBit.diameter = currentDrill->drill_size;
		curBit.unit = string( currentDrill->drill_unit );
		curBit.drill_count = currentDrill->drill_count;

		bits->insert( pair<int,drillbit>(currentDrill->drill_num, curBit) );
	}
}

void
ExcellonProcessor::parse_holes()
{
	if(!bits)
		parse_bits();

	holes = shared_ptr< map<int,icoords> >( new map<int,icoords>() );

	for (gerbv_net_t* currentNet = project->file[0]->image->netlist; currentNet; currentNet = currentNet->next) {
		if(currentNet->aperture != 0)
			(*holes)[currentNet->aperture].push_back( icoordpair(currentNet->start_x, currentNet->start_y) );
	}	
}

shared_ptr<const map<int,drillbit> >
ExcellonProcessor::get_bits()
{
	if(!bits)
		parse_bits();

	return bits;
}

shared_ptr<const map<int,icoords> >
ExcellonProcessor::get_holes()
{
	if(!holes)
		parse_holes();

	return holes;
}

ExcellonProcessor::~ExcellonProcessor()
{
	gerbv_destroy_project(project);
}
