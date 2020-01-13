#include <string>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>

namespace AllInOneConfig {

class ValidationProgramOptions {

    boost::program_options::options_description help, desc, hide, env;
    boost::program_options::positional_options_description pos_hide;

public:
    std::string config, key;
    //std::string CMSSW_BASE, CMSSW_RELEASE_BASE, SCRAM_ARCH;
    bool dry;

    ValidationProgramOptions ();
    void helper (int argc, char * argv[]);
    void parser (int argc, char * argv[]);
};

}
