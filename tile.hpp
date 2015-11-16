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
 
#ifndef TILE_H
#define TILE_H

#include <fstream>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include "common.hpp"
#include "unique_codes.hpp"

class Tiling
{
public:
    struct TileInfo
    {
        Software software;
        bool enabled;
        int tileX;
        int tileY;
        double boardWidth;
        double boardHeight;
        int tileVar;
        unsigned int forXNum;
        unsigned int forYNum;
    };

    Tiling( TileInfo tileInfo, double cfactor );
    void header( std::ofstream &of );
    void footer( std::ofstream &of );
    static TileInfo generateTileInfo( const boost::program_options::variables_map& options,
                                      uniqueCodes &ocodes, double boardHeight, double boardWidth );

    inline void setGCodeEnd( string _gCodeEnd )
    {
        gCodeEnd = _gCodeEnd;
    }
    
    inline string getGCodeEnd()
    {
        return gCodeEnd;
    }

    const TileInfo tileInfo;
    const double cfactor;
    unsigned int initialXOffsetVar;
    unsigned int initialYOffsetVar;

private:
    void tileSequence( std::ofstream &of );
    
    string gCodeEnd;
};

#endif // TILE_H
