/*!\defgroup MAIN*/
/******************************************************************************/
/*!
 \file       main.cpp
 \brief      Main module of pcb2gcode.

 \version
 04.12.2014 - Nicola Corna - nicola@corna.info\n
 - added preamble text option
 
 \version
 2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Added cut-front option.
 - Added onedrill option.
 - Added metricoutput option.
 - Formatted command line output.
 - Version option, now only outputs the package version.
 - Different code for unit (metric input) calculation.
 - Prepared documenting the code with doxygen.
 - Formatted the code.

 \version
 1.1.4 - 2009, 2010, 2011 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 and others

 \copyright
 pcb2gcode is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 pcb2gcode is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.

 \bug If a front and back file is given, the output is "empty".

 \ingroup    MAIN
 */
/******************************************************************************/

#include <iostream>
#include <fstream>

using std::cout;
using std::cerr;
using std::endl;
using std::fstream;

#include <glibmm/ustring.h>
using Glib::ustring;

#include <glibmm/init.h>
#include <gdkmm/wrap_init.h>

#include "gerberimporter.hpp"
#include "surface.hpp"
#include "ngc_exporter.hpp"
#include "board.hpp"
#include "drill.hpp"
#include "options.hpp"
#include "svg_exporter.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include <fstream>
#include <sstream>

/******************************************************************************/
/*
 */
/******************************************************************************/
int main(int argc, char* argv[]) {

   Glib::init();
   Gdk::wrap_init();

   options::parse(argc, argv);      //parse the command line parameters

   po::variables_map& vm = options::get_vm();      //get the cli parameters

   if (vm.count("version")) {      //return version and quit
      cout << PACKAGE_VERSION << endl;
      exit(0);
   }

   if (vm.count("help")) {      //return help and quit
      cout << options::help();
      exit(0);
   }

   options::check_parameters();      //check the cli parameters

   //---------------------------------------------------------------------------
   //deal with metric / imperial units for input parameters:

   double unit;      //!< factor for imperial/metric conversion

   unit = vm["metric"].as<bool>() ? (1. / 25.4) : 1;

   //---------------------------------------------------------------------------
   //prepare environment:

   shared_ptr<Isolator> isolator;

   if (vm.count("front") || vm.count("back")) {
      isolator = shared_ptr<Isolator>(new Isolator());
      isolator->tool_diameter = vm["offset"].as<double>() * 2 * unit;
      isolator->zwork = vm["zwork"].as<double>() * unit;
      isolator->zsafe = vm["zsafe"].as<double>() * unit;
      isolator->feed = vm["mill-feed"].as<double>() * unit;
      isolator->speed = vm["mill-speed"].as<int>();
      isolator->zchange = vm["zchange"].as<double>() * unit;
      isolator->extra_passes =
               vm.count("extra-passes") ? vm["extra-passes"].as<int>() : 0;
   }

   shared_ptr<Cutter> cutter;

   if (vm.count("outline") || (vm.count("drill") && vm.count("milldrill"))) {
      cutter = shared_ptr<Cutter>(new Cutter());
      cutter->tool_diameter = vm["cutter-diameter"].as<double>() * unit;
      cutter->zwork = vm["zcut"].as<double>() * unit;
      cutter->zsafe = vm["zsafe"].as<double>() * unit;
      cutter->feed = vm["cut-feed"].as<double>() * unit;
      cutter->speed = vm["cut-speed"].as<int>();
      cutter->zchange = vm["zchange"].as<double>() * unit;
      cutter->do_steps = true;
      cutter->stepsize = vm["cut-infeed"].as<double>() * unit;
   }

   shared_ptr<Driller> driller;

   if (vm.count("drill")) {
      driller = shared_ptr<Driller>(new Driller());
      driller->zwork = vm["zdrill"].as<double>() * unit;
      driller->zsafe = vm["zsafe"].as<double>() * unit;
      driller->feed = vm["drill-feed"].as<double>() * unit;
      driller->speed = vm["drill-speed"].as<int>();
      driller->zchange = vm["zchange"].as<double>() * unit;
   }

   //---------------------------------------------------------------------------
   //prepare custom preamble:

   string preamble, postamble;

   if (vm.count("preamble-text")) {
      cout << "Importing preamble text... ";
      string name = vm["preamble-text"].as<string>();
      fstream in(name.c_str(), fstream::in);

      if (!in.good()) {
         cerr << "Cannot read preamble-text file \"" << name << "\"" << endl;
         exit(1);
      }

      string tmp ((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
      boost::regex re ( "\\( *\\)" );

      boost::replace_all ( tmp, "(", "<" );       //Substitute round parenthesis with angled parenthesis
      boost::replace_all ( tmp, ")", ">" );
      boost::replace_all ( tmp, "\n", " )\n( " ); //Set the text as comment by adding round parenthesis

      preamble += boost::regex_replace( "( " + tmp + " )\n\n" , re, "");    //Remove empty comment blocks

      cout << "DONE\n";
   }

   if (vm.count("preamble")) {
      cout << "Importing preamble... ";
      string name = vm["preamble"].as<string>();
      fstream in(name.c_str(), fstream::in);

      if (!in.good()) {
         cerr << "Cannot read preamble file \"" << name << "\"" << endl;
         exit(1);
      }

      string tmp((std::istreambuf_iterator<char>(in)),
                 std::istreambuf_iterator<char>());
      preamble += tmp + "\n";
      cout << "DONE\n";
   }

   //---------------------------------------------------------------------------
   //prepare custom postamble:

   if (vm.count("postamble")) {
      cout << "Importing postamble... ";
      string name = vm["postamble"].as<string>();
      fstream in(name.c_str(), fstream::in);

      if (!in.good()) {
         cerr << "Cannot read postamble file \"" << name << "\"" << endl;
         exit(1);
      }

      string tmp((std::istreambuf_iterator<char>(in)),
                 std::istreambuf_iterator<char>());
      postamble = tmp + "\n";
      cout << "DONE\n";
   }

   //---------------------------------------------------------------------------

   shared_ptr<Board> board(
            new Board(
                     vm["dpi"].as<int>(),
                     vm.count("fill-outline"),
                     vm.count("fill-outline") ?
                              vm["outline-width"].as<double>() * unit :
                              INFINITY));

   // this is currently disabled, use --outline instead
   if (vm.count("margins")) {
      board->set_margins(vm["margins"].as<double>());
   }

   //--------------------------------------------------------------------------
   //load files, import layer files, create surface:

   try {

      //-----------------------------------------------------------------------
      cout << "Importing front side... ";

      try {
         string frontfile = vm["front"].as<string>();
         boost::shared_ptr<LayerImporter> importer(
                  new GerberImporter(frontfile));
         board->prepareLayer("front", importer, isolator, false,
                             vm.count("mirror-absolute"));
         cout << "DONE.\n";
      } catch (import_exception& i) {
         cout << "ERROR.\n";
      } catch (boost::exception& e) {
         cout << "not specified.\n";
      }

      //-----------------------------------------------------------------------
      cout << "Importing back side... ";

      try {
         string backfile = vm["back"].as<string>();
         boost::shared_ptr<LayerImporter> importer(
                  new GerberImporter(backfile));
         board->prepareLayer("back", importer, isolator, true,
                             vm.count("mirror-absolute"));
         cout << "DONE.\n";
      } catch (import_exception& i) {
         cout << "ERROR.\n";
      } catch (boost::exception& e) {
         cout << "not specified.\n";
      }

      //-----------------------------------------------------------------------
      cout << "Importing outline... ";

      //outline from front or back side:
      bool flip_outline = false;
      flip_outline =
               vm.count("cut-front") ? !vm["cut-front"].as<bool>() :
                                       !vm.count("front");           //ERROR??

      try {
         string outline = vm["outline"].as<string>();                               //Filename
         boost::shared_ptr<LayerImporter> importer(new GerberImporter(outline));
         board->prepareLayer("outline", importer, cutter, flip_outline,
                             vm.count("mirror-absolute"));

         cout << "DONE.\n";
      } catch (import_exception& i) {
         cout << "ERROR.\n";
      } catch (boost::exception& e) {
         cout << "not specified.\n";
      }

   } catch (import_exception& ie) {
      if (ustring const* mes = boost::get_error_info<errorstring>(ie))
         std::cerr << "Import Error: " << *mes;
      else
         std::cerr << "Import Error: No reason given.";
   }
   
   //---------------------------------------------------------------------------
   //SVG EXPORTER

   shared_ptr<SVG_Exporter> svgexpo(new SVG_Exporter(board));

   try {

     board->createLayers();      // throws std::logic_error

      if (vm.count("svg")) {
         cout << "Create SVG File ... " << vm["svg"].as<string>() << endl;
         svgexpo->create_svg(vm["svg"].as<string>());
      }

      shared_ptr<NGC_Exporter> exporter(new NGC_Exporter(board));
      exporter->add_header(PACKAGE_STRING);

      if (vm.count("preamble") || vm.count("preamble-text")) {
         exporter->set_preamble(preamble);
      }

      if (vm.count("postamble")) {
         exporter->set_postamble(postamble);
      }

      //SVG EXPORTER
      if (vm.count("svg")) {
         exporter->set_svg_exporter(svgexpo);
      }

      exporter->export_all(vm);

   } catch (std::logic_error& le) {
      cout << "Internal Error: " << le.what() << endl;
   } catch (std::runtime_error& re) {
      cout << "Runtime Error: " << re.what() << endl;
   }
   
   //---------------------------------------------------------------------------
   //load and process the drill file

   cout << "Importing drill... ";

   if (vm.count("drill")) {
      try {
         /*
         ExcellonProcessor ep(
                  vm["drill"].as<string>(),
                  board->get_max_x() - board->get_min_x(),
                  board->get_min_x()
                  + (board->get_max_x() - board->get_min_x()) / 2,
                  vm["metricoutput"].as<bool>());
*/
         ExcellonProcessor ep(
                  vm["drill"].as<string>(),
                  board->get_width(),
                  board->get_min_x() + board->get_width() / 2,
                  vm["metricoutput"].as<bool>());

         ep.add_header(PACKAGE_STRING);

         if (vm.count("preamble") || vm.count("preamble-text")) {
            ep.set_preamble(preamble);
         }

         if (vm.count("postamble")) {
            ep.set_postamble(postamble);
         }

         //SVG EXPORTER
         if (vm.count("svg")) {
            ep.set_svg_exporter(svgexpo);
         }

         cout << "DONE.\n";

         if (vm.count("milldrill")) {
            ep.export_ngc(vm["drill-output"].as<string>(), cutter,
                          !vm.count("drill-front"), vm.count("mirror-absolute"),
                          vm["onedrill"].as<bool>());
         } else {
            ep.export_ngc(vm["drill-output"].as<string>(), driller,
                          vm.count("drill-front"), vm.count("mirror-absolute"),
                          vm["onedrill"].as<bool>());
         }

      } catch (drill_exception& e) {
         cout << "ERROR.\n";
      }
   } else {
      cout << "not specified.\n";
   }
   cout << "END." << endl;

}
