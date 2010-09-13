
#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <stdexcept>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/noncopyable.hpp>

#include <istream>
#include <string>
using std::string;

class options : boost::noncopyable
{
public:
	static void parse_cl( int argc, char** argv );
	static void parse_files();

	static po::variables_map& get_vm() { return instance().vm; };
	static string help();

private:
	options();
	
	po::variables_map vm;

	po::options_description cli_options; //! CLI options
	po::options_description cfg_options; //! generic options

	static options& instance();
};


#endif // OPTIONS_HPP
