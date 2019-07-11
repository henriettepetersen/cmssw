#include <cstdlib>

#include <iostream>
#include <vector>

#include <boost/exception/exception.hpp>
#include <boost/exception/diagnostic_information.hpp> 

#include "AllInOneConfig.h"

using namespace AllInOneConfig;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
/// function validateAlignment
///
/// Generates:
///  - condor configs, including dagman
///  - modified ini configs for each step
void validateAlignment (string file)
{
    // read the config with Property Tree library from Boost
    pt::ptree tree;
    pt::read_ini(file, tree);
    Config config(tree);
    config.Print();

    // 1) loop over validations/plots
    //
    // 2) loop over geometries
    //      - identify potential preexisting statements
    // 3) loop over IOVs
    //
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
int main (int argc, char * argv[])
{
    // exactly one argument should be given
    if (argc != 2) {
        cerr << "Syntax:" << argv[0] << " config" << endl;
        return EXIT_FAILURE;
    }
    string file = argv[1];

    // display helper in case
    if (file == "-h" || file == "--help" || file == "-help") {
        cout << "Syntax:" << argv[0] << " config" << endl;
        return EXIT_SUCCESS;
    }

    // otherwise, assume the argument is the name of a file and check if it exists
    if (!fs::exists(file)) {
        cout << "Didn't find configuration file " << file << endl;
        return EXIT_FAILURE;
    }

    // validate, and catch exception if relevant
    try {
        validateAlignment(file);
    }
    // general boost exception
    catch( boost::exception & e ) {
        std::cerr << "Boost exception: " << boost::diagnostic_information(e);
        throw;
    }
    // boost property tree exceptions
    catch (pt::ptree_bad_data & e) {
        cerr << "Property Tree Bad Data: " << e.data<string>() << endl;
        return EXIT_FAILURE;
    }
    catch (pt::ptree_bad_path & e) {
        cerr << "Property Tree Bad Path: " << e.path<string>() << endl;
        return EXIT_FAILURE;
    }
    catch (pt::ptree_error & e) {
        cerr << "Property Tree Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    // standard exception
    catch (exception & e) {
        cerr << "Standard Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    catch (...) { 
        cerr << "Unkown failure" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
#endif
