/*!\defgroup OUTLINE_BRIDGES*/
/******************************************************************************/
/*!
 \file       outline_bridges.cpp
 \brief

 \version
 09.02.2015 - Nicola Corna - nicola@corna.info

 \copyright  pcb2gcode is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 pcb2gcode is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.

 \ingroup    SURFACE
 */
/******************************************************************************/

#include "outline_bridges.hpp"

#include <boost/bind.hpp>
#include <boost/assign/list_of.hpp>
#include <cmath>

vector<unsigned int> outline_bridges::makeBridges ( shared_ptr<icoords> &path, unsigned int number, double length ) {
    return insertBridges( path, findLongestSegments( path, number, length ), length );
}

//This function finds the longest segments and returns a vector of pair containing the index of path where the segment
//starts and its length. If no segments longer than "length" can be found, it throws outline_bridges_exception
vector< pair< unsigned int, double > > outline_bridges::findLongestSegments ( const shared_ptr<icoords> path, unsigned int number, double length ) {
    vector< pair< unsigned int, double > >::iterator element;
    vector< pair< unsigned int, double > > distances;
    vector< pair< unsigned int, double > > output;
    
    for( unsigned int i = 0; i < path->size() - 1; i++ )
        distances.push_back( std::make_pair( i, pointDistance( path->at(i), path->at(i+1) ) ) );

    for( unsigned int i = 0; i < number; i++ )
    {
        element = std::max_element( distances.begin(), distances.end(), boost::bind(&std::pair<unsigned int, double>::second, _1) <
                                                                        boost::bind(&std::pair<unsigned int, double>::second, _2) );  //Find the longest segment
        if( element->second < length || element == distances.end() ) {    //If it isn't long enough, or if there aren't segments
            if( output.empty() )
                throw outline_bridges_exception();  //Throw an exception if no bridges can be created
            else
                break;  //Stop looking for bridges and use the ones that can be used
        }
        output.push_back( *element );    //"save" the iterator
        distances.erase( element );      //Remove the element from the vector
    }

    return output;
}

//This function takes the segments where the bridges must be built (in the form of vector<pair<uint,double>>, see findLongestSegments),
//inserts them in the path and returns an array containing the indexes of each bridge's start
vector<unsigned int> outline_bridges::insertBridges ( shared_ptr<icoords> path, vector< pair< unsigned int, double > > chosenSegments, double length ) {
    vector<unsigned int> output;
    icoords temp;
    
    path->reserve( path->size() + chosenSegments.size() * 2 );  //Just to avoid unnecessary reallocations
    std::sort( chosenSegments.begin(), chosenSegments.end(), boost::bind(&pair<unsigned int, double>::first, _1) <
                                                             boost::bind(&pair<unsigned int, double>::first, _2) ); //Sort it (lower index -> higher index)

    for( unsigned int i = 0; i < chosenSegments.size(); i++ ) {
        chosenSegments[i].first += 2 * i;   //Each time we insert a bridge all following indexes have a offset of 2, we compensate it
        temp = boost::assign::list_of( intermediatePoint( path->at( chosenSegments[i].first ), path->at( chosenSegments[i].first + 1 ),
                                                          0.5 - ( length / chosenSegments[i].second ) / 2 ) )
                                     ( intermediatePoint( path->at( chosenSegments[i].first ), path->at( chosenSegments[i].first + 1 ),
                                                          0.5 + ( length / chosenSegments[i].second ) / 2 ) );
        path->insert( path->begin() + chosenSegments[i].first + 1, temp.begin(), temp.end() );  //Insert the bridges in the path
        output.push_back( chosenSegments[i].first + 1 );    //Add the bridges indexes to output
    }

    return output;
}

//This function compute the distance between two points
double outline_bridges::pointDistance( icoordpair p0, icoordpair p1 ) {
	double x1_x0 = p1.first - p0.first;
	double y1_y0 = p1.second - p0.second;
	
	return sqrt( x1_x0 * x1_x0 + y1_y0 * y1_y0 );
}

//This function returns the intermediate point between p0 and p1. With position=0 it returns p0, with position=1 it returns p1,
//with values between 0 and 1 it returns the relative position between p0 and p1
icoordpair outline_bridges::intermediatePoint( icoordpair p0, icoordpair p1, double position ) {
    return icoordpair( p0.first + ( p1.first - p0.first ) * position, p0.second + ( p1.second - p0.second ) * position );
}
