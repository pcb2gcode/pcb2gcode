/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2015 Nicola Corna <nicola@corna.info>
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

#include "outline_bridges.hpp"

#include <boost/assign/list_of.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <cmath>

vector<unsigned int> outline_bridges::makeBridges (shared_ptr<icoords> &path, unsigned int number, double length) {
  return insertBridges( path, findLongestSegments(path, number, length), length);
}

//This function finds the longest segments and returns a vector of pair containing the index of path where the segment
//starts and its length. If no segments longer than "length" can be found, it throws outline_bridges_exception
vector< pair< unsigned int, double > > outline_bridges::findLongestSegments ( const shared_ptr<icoords> path, unsigned int number, double length ) {
  if (number == 0) {
    return {}; // Saves time in the case of 0.
  }
  vector< pair< unsigned int, double > >::iterator element;
  vector< pair< unsigned int, double > > distances;
  vector< pair< unsigned int, double > > output;
  auto compare_2nd = [](pair<unsigned int, double> a, pair<unsigned int, double> b) { return a.second < b.second; };

  for( unsigned int i = 0; i < path->size() - 1; i++ )
    distances.push_back( std::make_pair( i, boost::geometry::distance( path->at(i), path->at(i+1) ) ) );

  for( unsigned int i = 0; i < number; i++ )
  {
    element = std::max_element( distances.begin(), distances.end(), compare_2nd );  //Find the longest segment
    if( element->second < length || element == distances.end() )  //If it isn't long enough, or if there aren't segments
    {
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
vector<unsigned int> outline_bridges::insertBridges ( shared_ptr<icoords> path, vector< pair< unsigned int, double > > chosenSegments, double length )
{
    vector<unsigned int> output;
    icoords temp (2);

    path->reserve( path->size() + chosenSegments.size() * 2 );  //Just to avoid unnecessary reallocations
    std::sort( chosenSegments.begin(), chosenSegments.end()); //Sort it (lower index -> higher index)

    for( unsigned int i = 0; i < chosenSegments.size(); i++ )
    {
        //Each time we insert a bridge all following indexes have a offset of 2, we compensate it
        chosenSegments[i].first += 2 * i;
        temp.at(0) = intermediatePoint( path->at( chosenSegments[i].first ),
                                        path->at( chosenSegments[i].first + 1 ),
                                        0.5 - ( length / chosenSegments[i].second ) / 2 );
        temp.at(1) = intermediatePoint( path->at( chosenSegments[i].first ),
                                        path->at( chosenSegments[i].first + 1 ),
                                        0.5 + ( length / chosenSegments[i].second ) / 2 );
        //Insert the bridges in the path
        path->insert( path->begin() + chosenSegments[i].first + 1, temp.begin(), temp.end() );
        output.push_back( chosenSegments[i].first + 1 );    //Add the bridges indexes to output
    }

    return output;
}

//This function returns the intermediate point between p0 and p1.
//With position=0 it returns p0, with position=1 it returns p1,
//with values between 0 and 1 it returns the relative position between p0 and p1
icoordpair outline_bridges::intermediatePoint( icoordpair p0, icoordpair p1, double position )
{
    return icoordpair( p0.first + ( p1.first - p0.first ) * position, p0.second + ( p1.second - p0.second ) * position );
}
