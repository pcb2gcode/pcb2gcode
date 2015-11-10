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

#ifndef UNIQUE_CODES_H
#define UNIQUE_CODES_H

class uniqueCodes
{
public:
    uniqueCodes(unsigned int start) :
        currentCode(start)
    {

    }
    
    inline unsigned int getUniqueCode()
    {
        return currentCode++;
    }

protected:
    unsigned int currentCode;
};

#endif // UNIQUE_CODES_H
