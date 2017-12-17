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

#include "geometry.hpp"
using std::pair;

using std::next;

class tsp_solver
{
private:
    enum class Side { FRONT, BACK };
    // You can extend this class adding new overloads of get with this prototype:
    //  icoordpair get(T _name_, Side side) { ... }
    //  icoordpair reverse(T _name_) { ... }
    static inline icoordpair get(icoordpair point, Side side)
    {
        return point;
    }

    static inline void reverse(icoordpair& point) {
        return;
    }

    static inline icoordpair get(shared_ptr<icoords> path, Side side)
    {
        if (side == Side::FRONT) {
            return path->front();
        } else {
            return path->back();
        }
    }

    static inline void reverse(shared_ptr<icoords>& path) {
        std::reverse(path->begin(), path->end());
    }

    static inline icoordpair get(ilinesegment line, Side side)
    {
        if (side == Side::FRONT) {
            return line.first;
        } else {
            return line.second;
        }
    }

    static inline void reverse(ilinesegment& line) {
        std::swap(line.first, line.second);
    }
    // Return the Chebyshev distance, which is a good approximation
    // for the time it takes to do a rapid move on a CNC router.
    static inline double distance(icoordpair p0, icoordpair p1)
    {
        return std::max(std::abs(p0.first - p1.first),
                        std::abs(p0.second - p1.second));
    }
public:
    // This function computes the optimised path of a
    //  * icoordpair
    //  * shared_ptr<icoords>
    // In the case of icoordpair it interprets the coordpairs as coordinates and computes the optimised path
    // In the case of shared_ptr<icoords> it interprets the vector<icoordpair> as closed paths, and it computes
    // the optimised path of the first point of each subpath. This can be used in the milling paths, where each
    // subpath is closed and we want to find the best subpath order
    template <typename T>
    static void nearest_neighbour(vector<T> &path, icoordpair startingPoint)
    {
        if (path.size() > 0)
        {
            list<T> temp_path (path.begin(), path.end());
            vector<T> newpath;
            double original_length;
            double new_length;
            double minDistance;
            unsigned int size = path.size();

            //Reserve memory
            newpath.reserve(size);

            new_length = 0;

            //Find the original path length
            original_length = distance(startingPoint, get(temp_path.front(), Side::FRONT));
            for (auto point = temp_path.begin(); next(point) != temp_path.end(); point++)
                original_length += distance(get(*point, Side::BACK), get(*next(point), Side::FRONT));

            icoordpair currentPoint = startingPoint;
            while (temp_path.size() > 0)
            {
                minDistance = distance(currentPoint, get(*(temp_path.begin()), Side::FRONT));
                auto nearestPoint = temp_path.begin();
                Side side = Side::FRONT;
                //Compute all the distances
                for (auto i = temp_path.begin(); i != temp_path.end(); i++) {
                    if (distance(currentPoint, get(*i, Side::FRONT)) < minDistance) {
                        minDistance = distance(currentPoint, get(*i, Side::FRONT));
                        nearestPoint = i;
                        side = Side::FRONT;
                    }
                    if (distance(currentPoint, get(*i, Side::BACK)) < minDistance) {
                        minDistance = distance(currentPoint, get(*i, Side::BACK));
                        nearestPoint = i;
                        side = Side::BACK;
                    }
                }

                new_length += distance(currentPoint, get(*(nearestPoint), side)); //Update the new path total length
                if (side == Side::BACK) {
                    reverse(*nearestPoint);
                }
                newpath.push_back(*(nearestPoint)); //Copy the chosen point into newpath
                currentPoint = get(*(nearestPoint), Side::BACK); //Set the next currentPoint to the chosen point
                temp_path.erase(nearestPoint);           //Remove the chosen point from the path list
            }

            if (new_length < original_length)  //If the new path is better than the previous one
                path = newpath;
        }
    }

    // Same as nearest_neighbor but afterwards does 2opt optimizations.
    template <typename T>
    static void tsp_2opt(vector<T> &path, icoordpair startingPoint, double quantization_error) {
        // Perform greedy on path if it improves.
        nearest_neighbour(path, startingPoint, quantization_error);
        bool found_one = true;
        while (found_one) {
            found_one = false;
            for (auto a = path.begin(); a < path.end(); a++) {
                auto b = a+1;
                for (auto c = b+1; c+1 < path.end(); c++) {
                    auto d = c+1;
                    // Should we make this 2opt swap?
                    if (boost::geometry::distance(get(*a), get(*b)) +
                        boost::geometry::distance(get(*c), get(*d)) >
                        boost::geometry::distance(get(*a), get(*c)) +
                        boost::geometry::distance(get(*b), get(*d))) {
                        // Do the 2opt swap.
                        std::reverse(b,d);
                        found_one = true;
                    }
                }
            }
        }
    }
};

#endif
