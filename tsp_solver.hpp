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

#ifndef TSP_HPP
#define TSP_HPP

#include <vector>
using std::vector;

#include <list>
using std::list;

#include <memory>
using std::shared_ptr;

#include "coord.hpp"
using std::pair;

class tsp_solver
{
private:
    // You can extend this class adding new overloads of get with this prototype:
    //  icoordpair get( T _name_ ) { ... }
    static inline icoordpair get( icoordpair point ) { return point; }
    static inline icoordpair get( shared_ptr<icoords> path ) { return path->front(); }

public:
    // This function computes the optimised path of a
    //  * icoordpair
    //  * shared_ptr<icoords>
    // In the case of icoordpair it interprets the coordpairs as coordinates and computes the optimised path
    // In the case of shared_ptr<icoords> it interprets the vector<icoordpair> as closed paths, and it computes
    // the optimised path of the first point of each subpath. This can be used in the milling paths, where each
    // subpath is closed and we want to find the best subpath order
    template <typename T> static void nearest_neighbour( vector<T> &path, icoordpair startingPoint, double quantization_error )
    {
        list<T> temp_path;
        vector<T> newpath;
        vector<double> distances;
        list< pair< vector<double>::iterator, typename list<T>::iterator > > nearestPoints; //Do we have to use a vector or a list here?
        typename vector<T>::iterator i;
        double original_length;
        double new_length;
        double minDistance;
        unsigned int size = path.size();

        //Reserve memory
        distances.reserve( size );
        newpath.reserve( size );

        temp_path.assign( path.begin(), path.end() );
        new_length = 0;

        //Find the original path length
        original_length = boost::geometry::distance( startingPoint, get(temp_path.front()) );
        for( typename list<T>::const_iterator point = boost::next(temp_path.begin()); point != temp_path.end(); point++ )
            original_length += boost::geometry::distance( get(*boost::prior(point)), get(*point) );

        icoordpair currentPoint = startingPoint;
        while( temp_path.size() > 1 )
        {

            //Compute all the distances
            for( typename list<T>::const_iterator i = temp_path.begin(); i != temp_path.end(); i++ )
                distances.push_back( boost::geometry::distance( currentPoint, get(*i) ) );

            //Find the minimum distance
            minDistance = *min_element( distances.begin(), distances.end() );

            //Find all the minimum distance points and copy their iterators in nearestPoints
            typename list<T>::iterator point = temp_path.begin();
            for( vector<double>::iterator dist = distances.begin(); dist != distances.end(); dist++ )
            {
                if( *dist - minDistance <= 2 * quantization_error )
                {
                    nearestPoints.push_back( make_pair( dist, point ) );
                }
                ++point;
            }

            typename list< pair< vector<double>::iterator, typename list<T>::iterator > >::iterator chosenPoint;
            if( nearestPoints.size() == 1 )
            {
                //Simplest case: the minimum distance point is unique; just copy it into newpath
                chosenPoint = nearestPoints.begin();
            }
            else
            {
                //More complex case: we have multiple minimum distance points (like in a grid); we have
                //to choose one of them
                chosenPoint = nearestPoints.begin(); //TODO choose in a smarter way
            }

            newpath.push_back( *(chosenPoint->second) ); //Copy the chosen point into newpath
            currentPoint = get(*(chosenPoint->second));        //Set the next currentPoint to the chosen point
            temp_path.erase( chosenPoint->second );           //Remove the chosen point from the path list
            new_length += *(chosenPoint->first);         //Update the new path total length
            distances.clear();                          //Clear the distances vector
            nearestPoints.clear();                      //Clear the nearestPoints vector
        }

        newpath.push_back( temp_path.front() );    //Copy the last point into newpath
        new_length += boost::geometry::distance( currentPoint, get(temp_path.front()) ); //Compute the distance and add it to new_length

        if( new_length < original_length )  //If the new path is better than the previous one
            path = newpath;
    }
};

#endif
