#include <string>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>

class ValidationProgramOptions {

    boost::program_options::options_description help, desc, hide;
    boost::program_options::positional_options_description pos_hide;

public:
    // TODO: use optional<const T>?
    std::string config;
    bool dry;

    ValidationProgramOptions ();
    void helper (int argc, char * argv[]);
    void parser (int argc, char * argv[]);
};
