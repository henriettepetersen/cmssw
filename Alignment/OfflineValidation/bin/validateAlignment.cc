#include <cstdlib>

#include <iostream>
#include <fstream>
#include <iomanip>

#include <string>
#include <numeric>

#include <boost/exception/exception.hpp>
#include <boost/exception/diagnostic_information.hpp> 

#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <boost/program_options.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

#include <experimental/filesystem>

namespace pt = boost::property_tree;
namespace po = boost::program_options;
namespace fs = std::experimental::filesystem;

using namespace std;

// just a "translator" to get the IOVs from a space-separated string into a vector of integers
template<typename T> class Tokenise {

    T Cast (string& s) const;

public:
    typedef string internal_type; // mandatory to define translator
    typedef vector<T> external_type; // id.

    boost::optional<external_type> get_value (const string &s) const // name is mandatory for translator
    {
        boost::char_separator<char> sep{" "};
        boost::tokenizer<boost::char_separator<char>> tok{s,sep};

        Tokenise::external_type results; 
        for (const auto& t: tok) {
            auto tt = boost::trim_copy(t);
            T ttt = Cast(tt);
            results.push_back(ttt);
        }

        return results;
    }

};
template<> string Tokenise<string>::Cast (string& s) const { return s; }
template<> int Tokenise<int>::Cast (string& s) const { return stoi(s); }

static const Tokenise<int   > tok_int;
static const Tokenise<string> tok_str;

// Example of DAGMAN:
//
// ```
// JOB jobname1 condor.sub
// VARS jobname1 key=value1
//
// JOB jobname2 condor.sub
// VARS jobname2 key=value2
//
// JOB merge merge.sub
// VARS merge otherkey=value3
//
// PARENT jobname1 jobname2 CHILD merge
// ```

struct Job {

    const string name, exec;
    const fs::path subdir;
    pt::ptree tree;

    vector<Job*> parents;

    Job (string Name, string Exec, fs::path Subdir) :
        name(Name), exec(Exec), subdir(Subdir)
    {
        cout << "Declaring job " << name << " to be run with " << exec << endl;
    }
};

ostream& operator<< (ostream& dag, const Job& job)
{
    dag << "JOB " << job.name << " condor.sub" << '\n'
        << "VARS " << job.name << " exec=" << job.exec << '\n';

    create_directories(job.subdir);
    pt::write_info(job.subdir / "config.info", job.tree);
    // TODO: copy condor.sub?

    if (job.parents.size() > 0)
        dag << accumulate(job.parents.begin(), job.parents.end(), string("PARENT"),
                [](string instruction, Job* parent) { return instruction + ' ' + parent->name; })
            << " CHILD " << job.name << '\n';

    return dag;
}

////////////////////////////////////////////////////////////////////////////////
/// function validateAlignment
///
/// Generates:
///  - condor configs, including dagman
///  - modified configs for each step
/// + submit validation
///
/// Documentation about Boost Property Trees
///  - https://www.boost.org/doc/libs/1_70_0/doc/html/property_tree.html
///  - https://theboostcpplibraries.com/boost.propertytree
int validateAlignment
                    (string file, //!< config file (INFO format)
                     bool dry) //!< if dry, then nothing gets submitted
{
    cout << "Starting validation" << endl;

    // check that the config file exists
    if (!fs::exists(file)) {
        cerr << "Didn't find configuration file " << file << '\n';
        exit(EXIT_FAILURE);
    }

    // - read the config with Property Tree library from Boost
    // - we use INFO type, which is very simple and especially designed
    //   for Property Trees
    // -> don't even need a dedicated parser!!

    cout << "Reading the config " << file << endl;
    pt::ptree main;
    pt::read_info(file, main);
    const fs::path dir = main.get<string>("name");
    const fs::path LFS = main.get<string>("LFS");

    if (fs::exists(dir))  {
        cerr << dir << " already exists\n";
        exit(EXIT_FAILURE);
    }
    cout << "Creating directory " << dir << endl;
    fs::create_directory(dir);

    cout << "Opening dag file" << endl;
    ofstream dag;
    dag.open(dir / "dag");

    cout << "Generating GCP configuration files" << endl;
    for (pair<string,pt::ptree> it: main.get_child("validations.GCP")) {

        string name = it.first;
        cout << name << endl;

        vector<int> IOVs = it.second.get<vector<int>>("IOVs", tok_int);
        it.second.erase("IOVs");

        for (int IOV: IOVs) {
            it.second.put<int>("IOV", IOV);

            fs::path subdir = dir / fs::path("GCP") / name / fs::path(to_string(IOV));
            cout << "The validation will be performed in " << subdir << endl;

            cout << "Declaring job" << endl;
            Job job("GCP_" + name + '_' + to_string(IOV), "GCP", subdir);

            string twig_ref  = "alignments." + it.second.get<string>("reference"),
                   twig_test = "alignments." + it.second.get<string>("test");

            job.tree.put<string>("LFS", LFS.c_str());
            job.tree.put_child(twig_ref, main.get_child(twig_ref));
            job.tree.put_child(twig_test, main.get_child(twig_test));
            job.tree.put_child(name, it.second);

            cout << "Queuing job" << endl;
            dag << job;
        }
    }

    cout << "Generating DMR configuration files" << endl;
    for (pair<string,pt::ptree> it: main.get_child("validations.DMR.single")) {

        string name = it.first;
        cout << name << endl;

        vector<int> IOVs = it.second.get<vector<int>>("IOVs", tok_int);
        it.second.erase("IOVs");

        for (int IOV: IOVs) {
            it.second.put<int>("IOV", IOV);

            fs::path subdir = dir / fs::path("DMR/single") / name / fs::path(to_string(IOV));
            cout << "The validation will be performed in " << subdir << endl;

            cout << "Declaring job" << endl;
            Job job("DMR_single_" + name + '_' + to_string(IOV), "DMR", subdir);

            job.tree.put<string>("LFS", LFS.c_str());

            vector<string> alignments = it.second.get<vector<string>>("alignments", tok_str);
            for (string& alignment: alignments) {
                string twig = "alignments." + alignment;
                job.tree.put_child(twig, main.get_child(twig));
            }
            job.tree.put_child(name, it.second);

            cout << "Queuing job" << endl;
            dag << job;
        }
    }

    //Job single1(dir / "single1", tree, "DMRsingle"),
    //    single2(dir / "single2", tree, "DMRsingle");
    //Job merge(dir / "merge", tree, "DMRmerge");
    //merge.parents = {&single1, &single2};
    //dag << single1 << single2 << merge;

    cout << "Closing dag file" << endl;
    dag.close();

    if (dry) {
        cout << "Dry run, exiting now" << endl;
        return EXIT_SUCCESS;
    }

    cout << "Submitting" << endl;
    return system("echo condor_submit_dat blah"); // TODO: remove `echo`
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// The main function contains:
//  - the parsing of the arguments with Boost Program Options
//  - the handling of the exceptions/errors
// However, the real "main" function containing the essence of the program
// is in `validateAlignment`
int main (int argc, char * argv[])
{
    try {
        string config;
        bool dry;
        try {
            // define all options
            po::options_description help{"Helper"}, desc{"Options"}, hide{"Hidden"};
            help.add_options()
                ("help,h", "Help screen")
                ("example,e", "Print example of config")
                ("tutorial,t", "Explain how to use the command");
            desc.add_options()
                ("dry,d", po::value<bool>(&dry)->default_value(false), "Set up all configs and jobs, but don't submit")
                ("verbose,v", po::bool_switch()->default_value(false)->notifier([](bool flag) { if (!flag) cout.setstate(ios_base::failbit); }), "Enable standard output stream")
                ("silent,s" , po::bool_switch()->default_value(false)->notifier([](bool flag) { if ( flag) cerr.setstate(ios_base::failbit); }), "Disable standard error stream");
            hide.add_options()
                ("config,c", po::value<string>(&config)->required(), "INFO config file");
            po::positional_options_description pos_hide;
            pos_hide.add("config", 1);

            // 1) treat the helper
            {
                po::options_description options;
                options.add(help).add(desc); 

                po::command_line_parser parser_helper{argc, argv};
                parser_helper.options(help) // don't parse required options before parsing help
                             .allow_unregistered(); // ignore unregistered options,

                // defines actions
                po::variables_map vm;
                po::store(parser_helper.run(), vm);
                po::notify(vm); // necessary for config to be given the value from the cmd line

                if (vm.count("help")) {
                    cout << "Basic syntax:\n  " << argv[0] << " config.info\n"
                         << options << endl;
                    return EXIT_SUCCESS;
                }

                if (vm.count("tutorial") || vm.count("example")) { // TODO
                    cout << "Example:\n"
                         << "   ______________________\n"
                         << "  < Oops! not ready yet! >\n"
                         << "   ----------------------\n"
                         <<"          \\   ^__^\n"
                         <<"           \\  (oo)\\_______\n"
                         << "              (__)\\       )\\/\\\n"
                         << "                  ||----w |\n"
                         << "                  ||     ||\n"
                         << flush;
                    return EXIT_SUCCESS;
                }
            }

            // 2) now parse all other options
            {
                po::options_description cmd_line;
                cmd_line.add(desc).add(hide);

                po::command_line_parser parser{argc, argv};
                parser.options(cmd_line)
                      .positional(pos_hide);

                po::variables_map vm;
                po::store(parser.run(), vm);
                po::notify(vm); // necessary for config to be given the value from the cmd line

                // TODO
                // Boost.ProgramOptions also defines the function boost::program_options::parse_environment(),
                // which can be used to load options from environment variables.
                // The class boost::environment_iterator lets you iterate over environment variables.
            }
        }
        catch (const po::required_option &e) {
            if (e.get_option_name() == "--config")
                cerr << "Missing config" << '\n';
            else 
                cerr << "Program Options Required Option: " << e.what() << '\n';
            return EXIT_FAILURE;
        }
        catch (const po::invalid_syntax &e) {
            cerr << "Program Options Invalid Syntax: " << e.what() << '\n';
            return EXIT_FAILURE;
        }
        catch (const po::error &e) {
            cerr << "Program Options Error: " << e.what() << '\n';
            return EXIT_FAILURE;
        }

        // validate, and catch exception if relevant
        try {
            return validateAlignment(config, dry);
        }
        catch (pt::ptree_bad_data & e) {
            cerr << "Property Tree Bad Data Error: " << e.data<string>() << '\n';
            return EXIT_FAILURE;
        }
        catch (pt::ptree_bad_path & e) {
            cerr << "Property Tree Bad Path Error: " << e.path<string>() << '\n';
            return EXIT_FAILURE;
        }
        catch (pt::ptree_error & e) {
            cerr << "Property Tree Error: " << e.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    // general boost exception
    catch( boost::exception & e ) {
        cerr << "Boost exception: " << boost::diagnostic_information(e);
        return EXIT_FAILURE;
    }
    // standard exception
    catch (logic_error & e) {
        cerr << "Logic Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (exception & e) {
        cerr << "Standard Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...) { 
        cerr << "Unkown failure\n";
        return EXIT_FAILURE;
    }
}
#endif
