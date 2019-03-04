/*
 * This file is part of pcb2gcode.
 *
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others
 * Copyright (C) 2010 Bernhard Kubicek <kubicek@gmx.at>
 * Copyright (C) 2013 Erik Schuster <erik@muenchen-ist-toll.de>
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>

#include <vector>
using std::vector;

using std::cout;
using std::cerr;
using std::endl;
using std::flush;
using std::fstream;
using std::shared_ptr;

#include <string>
using std::string;

#include <glibmm/ustring.h>
using Glib::ustring;

#include <glibmm/init.h>
#include <gdkmm/wrap_init.h>

#include <glibmm/miscutils.h>
using Glib::build_filename;

#include "gerberimporter.hpp"
#include "ngc_exporter.hpp"
#include "board.hpp"
#include "drill.hpp"
#include "options.hpp"
#include "units.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/version.hpp>

void do_pcb2gcode(int argc, const char* argv[]) {
    Glib::init();
    Gdk::wrap_init();

    options::parse(argc, argv);      //parse the command line parameters

    po::variables_map& vm = options::get_vm();      //get the cli parameters

    if (vm.count("version")) {       //return version and quit
      cout << PACKAGE_VERSION << endl;
      cout << "Git commit: " << GIT_VERSION << endl;
      cout << "Boost: " << BOOST_VERSION << endl;
      cout << "Gerbv: " << GERBV_VERSION << endl;
      return;
    }

    if (vm.count("help")) {       //return help and quit
      cout << options::help();
      return;
    }

    options::check_parameters();      //check the cli parameters

    //---------------------------------------------------------------------------
    //deal with metric / imperial units for input parameters:

    double unit;      //factor for imperial/metric conversion

    unit = vm["metric"].as<bool>() ? (1. / 25.4) : 1;

    //---------------------------------------------------------------------------
    //prepare environment:

    const double tolerance = vm["tolerance"].as<double>() * unit;
    const bool explicit_tolerance = !vm["nog64"].as<bool>();
    const string outputdir = vm["output-dir"].as<string>();
    const double spindown_time = vm.count("spindown-time") ?
        vm["spindown-time"].as<Time>().asMillisecond(1) : vm["spinup-time"].as<Time>().asMillisecond(1);
    shared_ptr<Isolator> isolator;

    if (vm.count("front") || vm.count("back"))
    {
        isolator = shared_ptr<Isolator>(new Isolator());
        // TODO: support more than one mill-diameter.
        for (const auto& tool_diameter : flatten(vm["mill-diameters"].as<std::vector<CommaSeparated<Length>>>())) {
          isolator->tool_diameters_and_overlap_widths.push_back(
              std::make_pair(
                  tool_diameter.asInch(unit),
                  boost::apply_visitor(percent_visitor(tool_diameter), vm["milling-overlap"].as<boost::variant<Length, Percent>>()).asInch(unit)));
        }
        isolator->voronoi = vm["voronoi"].as<bool>();
        isolator->zwork = vm["zwork"].as<Length>().asInch(unit);
        isolator->zsafe = vm["zsafe"].as<Length>().asInch(unit);
        isolator->feed = vm["mill-feed"].as<Velocity>().asInchPerMinute(unit);
        if (vm.count("mill-vertfeed"))
            isolator->vertfeed = vm["mill-vertfeed"].as<Velocity>().asInchPerMinute(unit);
        else
            isolator->vertfeed = isolator->feed / 2;
        isolator->speed = vm["mill-speed"].as<Rpm>().asRpm(1);
        isolator->zchange = vm["zchange"].as<Length>().asInch(unit);
        isolator->extra_passes = vm["extra-passes"].as<int>();
        isolator->isolation_width = vm["isolation-width"].as<Length>().asInch(unit);
        isolator->optimise = vm["optimise"].as<bool>();
        isolator->preserve_thermal_reliefs = vm["preserve-thermal-reliefs"].as<bool>();
        isolator->eulerian_paths = vm["eulerian-paths"].as<bool>();
        isolator->path_finding_limit = vm["path-finding-limit"].as<size_t>();
        isolator->g0_vertical_speed = vm["g0-vertical-speed"].as<Velocity>().asInchPerMinute(unit);
        isolator->g0_horizontal_speed = vm["g0-horizontal-speed"].as<Velocity>().asInchPerMinute(unit);
        isolator->tolerance = tolerance;
        isolator->explicit_tolerance = explicit_tolerance;
        isolator->pre_milling_gcode = boost::algorithm::join(
            vm["pre-milling-gcode"].as<vector<string>>(), "\n");
        isolator->post_milling_gcode = boost::algorithm::join(
            vm["post-milling-gcode"].as<vector<string>>(), "\n");
        isolator->spinup_time = vm["spinup-time"].as<Time>().asMillisecond(1);
        isolator->spindown_time = spindown_time;
    }

    shared_ptr<Cutter> cutter(new Cutter());

    if (vm.count("outline") ||
        (vm.count("drill") &&
         vm["min-milldrill-hole-diameter"].as<Length>() < Length(std::numeric_limits<double>::infinity()))) {
      cutter->tool_diameter = vm["cutter-diameter"].as<Length>().asInch(unit);
      cutter->zwork = vm["zcut"].as<Length>().asInch(unit);
      cutter->zsafe = vm["zsafe"].as<Length>().asInch(unit);
      cutter->feed = vm["cut-feed"].as<Velocity>().asInchPerMinute(unit);
      if (vm.count("cut-vertfeed"))
        cutter->vertfeed = vm["cut-vertfeed"].as<Velocity>().asInchPerMinute(unit);
      else
        cutter->vertfeed = cutter->feed / 2;
      cutter->speed = vm["cut-speed"].as<Rpm>().asRpm(1);
      cutter->zchange = vm["zchange"].as<Length>().asInch(unit);
      cutter->stepsize = vm["cut-infeed"].as<Length>().asInch(unit);
      cutter->optimise = vm["optimise"].as<bool>();
      cutter->eulerian_paths = vm["eulerian-paths"].as<bool>();
      cutter->path_finding_limit = vm["path-finding-limit"].as<size_t>();
      cutter->g0_vertical_speed = vm["g0-vertical-speed"].as<Velocity>().asInchPerMinute(unit);
      cutter->g0_horizontal_speed = vm["g0-horizontal-speed"].as<Velocity>().asInchPerMinute(unit);
      cutter->tolerance = tolerance;
      cutter->explicit_tolerance = explicit_tolerance;
      cutter->spinup_time = vm["spinup-time"].as<Time>().asMillisecond(1);
      cutter->spindown_time = spindown_time;
      cutter->bridges_num = vm["bridgesnum"].as<unsigned int>();
      cutter->bridges_width = vm["bridges"].as<Length>().asInch(unit);
      if (vm.count("zbridges"))
        cutter->bridges_height = vm["zbridges"].as<Length>().asInch(unit);
      else
        cutter->bridges_height = cutter->zsafe;
    }

    shared_ptr<Driller> driller;

    if (vm.count("drill"))
    {
        driller = shared_ptr<Driller>(new Driller());
        driller->zwork = vm["zdrill"].as<Length>().asInch(unit);
        driller->zsafe = vm["zsafe"].as<Length>().asInch(unit);
        driller->feed = vm["drill-feed"].as<Velocity>().asInchPerMinute(unit);
        driller->speed = vm["drill-speed"].as<Rpm>().asRpm(1);
        driller->tolerance = tolerance;
        driller->explicit_tolerance = explicit_tolerance;
        driller->spinup_time = vm["spinup-time"].as<Time>().asMillisecond(1);
        driller->spindown_time = spindown_time;
        driller->zchange = vm["zchange"].as<Length>().asInch(unit);
    }

    //---------------------------------------------------------------------------
    //prepare custom preamble:

    string preamble, postamble;

    if (vm.count("preamble-text"))
    {
        cout << "Importing preamble text... " << flush;
        string name = vm["preamble-text"].as<string>();
        fstream in(name.c_str(), fstream::in);

        if (!in.good()) {
          options::maybe_throw("Cannot read preamble-text file \"" + name + "\"", ERR_INVALIDPARAMETER);
        }

        string line;
        string tmp;

        while (std::getline(in, line))
        {
            tmp = line;
            boost::erase_all(tmp, " ");
            boost::erase_all(tmp, "\t");

            if( tmp.empty() )		//If there's nothing but spaces and \t
                preamble += '\n';
            else
            {
                boost::replace_all ( line, "(", "<" );       //Substitute round parenthesis with angled parenthesis
                boost::replace_all ( line, ")", ">" );
                preamble += "( " + line + " )\n";
            }
        }

        cout << "DONE\n";
    }

    if (vm.count("preamble"))
    {
        cout << "Importing preamble... " << flush;
        string name = vm["preamble"].as<string>();
        fstream in(name.c_str(), fstream::in);

        if (!in.good()) {
          options::maybe_throw("Cannot read preamble file \"" + name + "\"", ERR_INVALIDPARAMETER);
        }

        string tmp((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
        preamble += tmp + "\n";
        cout << "DONE\n";
    }

    //---------------------------------------------------------------------------
    //prepare custom postamble:

    if (vm.count("postamble"))
    {
        cout << "Importing postamble... " << flush;
        string name = vm["postamble"].as<string>();
        fstream in(name.c_str(), fstream::in);

        if (!in.good()) {
          options::maybe_throw("Cannot read postamble file \"" + name + "\"", ERR_INVALIDPARAMETER);
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
            vm["fill-outline"].as<bool>(),
            outputdir,
            vm["tsp-2opt"].as<bool>(),
            vm["mill-feed-direction"].as<MillFeedDirection::MillFeedDirection>(),
            vm["invert-gerbers"].as<bool>(),
            !vm["draw-gerber-lines"].as<bool>()));

    // this is currently disabled, use --outline instead
    if (vm.count("margins"))
    {
        board->set_margins(vm["margins"].as<double>());
    }

    //--------------------------------------------------------------------------
    //load files, import layer files, create surface:

    cout << "Importing front side... " << flush;
    if (vm.count("front") > 0) {
      string frontfile = vm["front"].as<string>();
      shared_ptr<GerberImporter> importer(new GerberImporter());
      if (!importer->load_file(frontfile)) {
        options::maybe_throw("ERROR.", ERR_INVALIDPARAMETER);
      }
      board->prepareLayer("front", importer, isolator, false);
      cout << "DONE.\n";
    } else {
      cout << "not specified.\n";
    }

    cout << "Importing back side... " << flush;
    if (vm.count("back") > 0) {
      string backfile = vm["back"].as<string>();
      shared_ptr<GerberImporter> importer(new GerberImporter());
      if (!importer->load_file(backfile)) {
        options::maybe_throw("ERROR.", ERR_INVALIDPARAMETER);
      }
      board->prepareLayer("back", importer, isolator, true);
      cout << "DONE.\n";
    } else {
      cout << "not specified.\n";
    }

    cout << "Importing outline... " << flush;
    if (vm.count("outline") > 0) {
      string outline = vm["outline"].as<string>();
      shared_ptr<GerberImporter> importer(new GerberImporter());
      if (!importer->load_file(outline)) {
        options::maybe_throw("ERROR.", ERR_INVALIDPARAMETER);
      }
      board->prepareLayer("outline", importer, cutter, !workSide(vm, "cut"));
      cout << "DONE.\n";
    } else {
      cout << "not specified.\n";
    }

    Tiling::TileInfo *tileInfo = NULL;

    cout << "Processing input files... " << flush;
    board->createLayers();
    cout << "DONE.\n";

    if (!vm["no-export"].as<bool>()) {
      shared_ptr<NGC_Exporter> exporter(new NGC_Exporter(board));
      exporter->add_header(PACKAGE_STRING);

      if (vm.count("preamble") || vm.count("preamble-text")) {
        exporter->set_preamble(preamble);
      }

      if (vm.count("postamble")) {
        exporter->set_postamble(postamble);
      }

      exporter->export_all(vm);

      tileInfo = new Tiling::TileInfo;
      *tileInfo = exporter->getTileInfo();
    }

    //---------------------------------------------------------------------------
    //load and process the drill file

    cout << "Importing drill... " << flush;

    if (vm.count("drill") > 0) {
        try
        {
            icoordpair min;
            icoordpair max;

            //Check if there are layers in "board"; if not, we have to compute
            //the size of the board now, based only on the size of the drill layer
            //(the resulting drill gcode will be probably misaligned, but this is the
            //best we can do)
            if(board->get_layersnum() == 0)
            {
              shared_ptr<GerberImporter> importer(new GerberImporter());
              if (!importer->load_file(vm["drill"].as<string>())) {
                options::maybe_throw("ERROR.", ERR_INVALIDPARAMETER);
              }
                min = std::make_pair( importer->get_min_x(), importer->get_min_y() );
                max = std::make_pair( importer->get_max_x(), importer->get_max_y() );
            }
            else
            {
                min = std::make_pair( board->get_min_x(), board->get_min_y() );
                max = std::make_pair( board->get_max_x(), board->get_max_y() );
            }

            ExcellonProcessor ep(vm, min, max);

            ep.add_header(PACKAGE_STRING);

            if (vm.count("preamble") || vm.count("preamble-text"))
            {
                ep.set_preamble(preamble);
            }

            if (vm.count("postamble"))
            {
                ep.set_postamble(postamble);
            }

            cout << "DONE.\n";

            boost::optional<string> drill_filename = vm["drill-output"].as<string>();
            boost::optional<string> milldrill_filename = vm["milldrill-output"].as<string>();
            if (vm["no-export"].as<bool>())
            {
                drill_filename = boost::none;
                milldrill_filename = boost::none;
            }
            // We can modify the cutter because we're not going to use it again.
            if (vm.count("milldrill-diameter")) {
              cutter->tool_diameter = vm["milldrill-diameter"].as<Length>().asInch(unit);
            }
            if (vm.count("zmilldrill")) {
              cutter->zwork = vm["zmilldrill"].as<Length>().asInch(unit);
            } else {
              cutter->zwork = vm["zdrill"].as<Length>().asInch(unit);
            }
            ep.export_ngc(outputdir, milldrill_filename, cutter,
                          vm["zchange-absolute"].as<bool>());
            ep.export_ngc(outputdir, drill_filename,
                          driller, vm["onedrill"].as<bool>(), vm["nog81"].as<bool>(),
                          vm["zchange-absolute"].as<bool>());

            cout << "DONE. The board should be drilled from the " << ( workSide(vm, "drill") ? "FRONT" : "BACK" ) << " side.\n";

        }
        catch (const drill_exception& e) {
          options::maybe_throw("ERROR: drill_exception", ERR_INVALIDPARAMETER);
        }
    } else {
        cout << "not specified.\n";
    }

    cout << "END." << endl;

}

int main(int argc, const char* argv[]) {
  try {
    do_pcb2gcode(argc, argv);
  } catch (pcb2gcode_parse_exception e) {
    cerr << e.what() << endl;
    return e.code();
  }
  return 0;
}
