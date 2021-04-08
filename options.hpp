/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 * Copyright (C) 2014, 2015 Nicola Corna <nicola@corna.info>
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

#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <stdexcept>

#include <memory>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/noncopyable.hpp>

#include <istream>
#include <string>

enum ErrorCodes {
    ERR_OK = 0,
    ERR_NOZWORK = 1,
    ERR_NOCUTTERDIAMETER = 2,
    ERR_NOZSAFE = 3,
    ERR_NOOFFSET = 4,
    ERR_NOZCUT = 5,
    ERR_NOCUTFEED = 6,
    ERR_NOCUTSPEED = 7,
    ERR_NOCUTINFEED = 8,
    ERR_NOZDRILL = 9,
    ERR_NOZCHANGE = 10,
    ERR_NODRILLFEED = 11,
    ERR_NODRILLSPEED = 12,
    ERR_NOMILLFEED = 13,
    ERR_NOMILLSPEED = 14,
    ERR_ZSAFELOWERZWORK = 15,
    ERR_NEGATIVEMILLFEED = 16,
    ERR_NEGATIVEMILLSPEED = 17,
    ERR_ZSAFELOWERZDRILL = 18,
    ERR_ZSAFELOWERZCHANGE = 19,
    ERR_NEGATIVEDRILLFEED = 20,
    ERR_ZSAFELOWERZCUT = 21,
    ERR_NEGATIVECUTFEED = 22,
    ERR_NEGATIVESPINDLESPEED = 23,
    ERR_LOWCUTINFEED = 24,
    ERR_NOOUTLINEWIDTH = 25,
    ERR_NEGATIVEOUTLINEWIDTH = 26,
    ERR_ZEROOUTLINEWIDTH = 27,
    ERR_NEGATIVEDRILLSPEED = 28,
    ERR_NOSOFTWARE = 29,
    ERR_NOALX = 30,
    ERR_NOALY = 31,
    ERR_NOALPROBEFEED = 32,
    ERR_NEGATIVEBRIDGE = 33,
    ERR_BRIDGENOOPTIMISE = 34,
    ERR_NEGATIVEALX = 35,
    ERR_NEGATIVEALY = 36,
    ERR_NEGATIVEPROBEFEED = 37,
    ERR_NEGATIVECUTVERTFEED = 39,
    ERR_NEGATIVEMILLVERTFEED = 40,
    ERR_NEGATIVETILEX = 41,
    ERR_NEGATIVETILEY = 42,
    ERR_BOTHDRILLFRONTSIDE = 43,
    ERR_UNKNOWNDRILLSIDE = 44,
    ERR_BOTHCUTFRONTSIDE = 45,
    ERR_UNKNOWNCUTSIDE = 46,
    ERR_VORONOINOVECTORIAL = 47,
    ERR_VORONOINOOUTLINE = 48,
    ERR_BOTHTOLERANCEG64 = 49,
    ERR_NEGATIVETOLERANCE = 50,
    ERR_NEGATIVEZWORK = 51,
    ERR_NEGATIVESPINUP = 52,
    ERR_NEGATIVESPINDOWN = 53,
    ERR_FALSEMIRRORABSOLUTE = 54,
    ERR_LOWMILLINFEED = 55,
    ERR_INVALIDPARAMETER = 100,
    ERR_UNKNOWNPARAMETER = 101
};

class pcb2gcode_parse_exception : public std::exception {
 public:
  pcb2gcode_parse_exception(const std::string& what, ErrorCodes error_code) {
    what_string = what;
    this->error_code = error_code;
  }
  virtual const char* what() const throw() {
    return what_string.c_str();
  }
  virtual ErrorCodes code() const throw() {
    return error_code;
  }

 private:
  std::string what_string;
  ErrorCodes error_code;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class options : private boost::noncopyable
{

public:
    static void parse(int argc, const char** argv);
    static void parse_files();
    static void check_parameters();
    static po::variables_map& get_vm()
    {
        return instance().vm;
    }
    ;
    static std::string help();

    static void maybe_throw(const std::string& what, ErrorCodes error_code);
private:
    options();
    po::variables_map vm;
    po::options_description cli_options;      //CLI options
    po::options_description cfg_options;      // all the non-CLI options
    static options& instance();
};

#endif // OPTIONS_HPP
