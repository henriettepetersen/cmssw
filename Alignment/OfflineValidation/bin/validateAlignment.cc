#include <cstdlib>

// IO

#include <fstream>
#include <iostream>

// STL

#include <string>
#include <numeric>
#include <map>
//#include <any>
//#include <optional>
#include <experimental/filesystem>

using namespace std;
namespace fs = experimental::filesystem;

#include "validateAlignmentProgramOptions.h"

// boost

#include <boost/exception/exception.hpp>
#include <boost/exception/diagnostic_information.hpp> 

#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <boost/program_options/errors.hpp>

#include <boost/algorithm/string.hpp>

#include <boost/tokenizer.hpp>

namespace pt = boost::property_tree; // used to read the config file
namespace po = boost::program_options; // used to read the options from command line

// `boost::optional` allows to safely initialiase an object
// example:
// ```
// optional<int> i = someFunction();
// if (i) cout << *i << endl;
// ```
// NB: std::optional exists in C++17 but there are issues when using Property Trees...

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// just a "translator" to get the IOVs from a space-separated string into a vector of integers
template<typename T> class Tokenise {

    T Cast (string& s) const;

    public:
    typedef string internal_type; // mandatory to define translator
    typedef vector<T> external_type; // id.

    boost::optional<external_type> get_value (const internal_type &s) const // name is mandatory for translator
    {
        boost::char_separator<char> sep{" "};
        // TODO: replace all EOLs and multiple spaces by a single space
        boost::tokenizer<boost::char_separator<char>> tok{s,sep};

        external_type results; 
        for (const auto& t: tok) {
            auto tt = boost::trim_copy(t);
            T ttt = Cast(tt);
            //any tt = boost::trim_copy(t); // TODO: use any
            //T ttt = any_cast<T>(tt);
            results.push_back(ttt);
        }
        return results;
    }
};
template<> string Tokenise<string>::Cast (string& s) const { return s; }
template<> int Tokenise<int>::Cast (string& s) const { return stoi(s); }
#endif

static const Tokenise<int   > tok_int;
static const Tokenise<string> tok_str;

struct Job {

    const string name, exec;
    const fs::path subdir;
    pt::ptree tree;

    vector<string> parents;

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
                [](string instruction, string parent) { return instruction + ' ' + parent; })
            << " CHILD " << job.name << '\n';

    return dag;
}
// Example:
// ```
// Job single1(...), single2(...);
// Job merge(...);
// merge.parents = {&single1, &single2};
// dag << single1 << single2 << merge;
// ```

// Example of DAGMAN:
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

////////////////////////////////////////////////////////////////////////////////
/// class Validation
///
/// Generates:
///  - condor configs, including dagman
///  - modified configs for each step
///
/// The class is mainly define to split the code in several function, only for readability.
///
/// Documentation about Boost Property Trees
///  - https://www.boost.org/doc/libs/1_70_0/doc/html/property_tree.html
///  - https://theboostcpplibraries.com/boost.propertytree
class Validation {

    pt::ptree alignments; //!< subtree containing the alignments
    boost::optional<pt::ptree> GCP, DMR; //!< validation blocks
    fs::path dir, LFS; //!< path to directories where files are stored (dir for local config and plots, LSF for heavy root files)
    ofstream dag; //!< file to which the hierarchy of the job is written, submitted at the end of the program

    vector<int> GetIOVsFromTree (const pt::ptree& tree);

public:
    Validation //!< constructor
        (string file); //!< name of the INFO config file

    void PrepareGCP (); //!< configure Geometry Comparison Plotter
    void PrepareDMR (); //!< configure "offline validation", measuring the local performance
    // TODO: PV, etc.

    void CloseDag (); //!< close the DAG man
    int SubmitDag () const; //!< submit the DAG man
};

vector<int> Validation::GetIOVsFromTree (const pt::ptree& tree)
{
    if (tree.count("IOVs")) {
        cout << "Getting IOVs" << endl;
        if (tree.count("IOV")) {
            cerr << "There shouldn't be both 'IOV' and 'IOVs' statements in the same block\n"; // TODO: be more precise
            exit(EXIT_FAILURE);
        }
        vector<int> IOVs = tree.get<vector<int>>("IOVs", tok_int);
        if (!is_sorted(IOVs.begin(), IOVs.end())) {
            cerr << "IOVs are not sorted\n";
            exit(EXIT_FAILURE);
        }
        return IOVs;
    }
    else if (tree.count("IOV")) {
        cout << "Getting single IOV" << endl;
        return {tree.get<int>("IOV")};
    }
    else
        return vector<int>();
}

Validation::Validation (string file)
{
    cout << "Starting validation" << endl;

    cout << "Reading the config " << file << endl;
    pt::ptree main_tree;
    pt::read_info(file, main_tree);

    dir = main_tree.get<string>("name");
    LFS = main_tree.get<string>("LFS");

    alignments = main_tree.get_child("alignments");

    //if (!main_tree.count("validations")) throw pt::ptree_bad_path("No validation found", "validations");

    GCP = main_tree.get_child_optional("validations.GCP");
    DMR = main_tree.get_child_optional("validations.DMR");

    if (!GCP && !DMR) {
        cerr << "No validation found!\n";
        exit(EXIT_FAILURE);
    }

    if (fs::exists(dir))  {
        cerr << dir << " already exists\n";
        exit(EXIT_FAILURE);
    }
    cout << "Creating directory " << dir << endl;
    fs::create_directory(dir);

    cout << "Opening dag file" << endl;
    dag.open(dir / "dag");
}

void Validation::PrepareGCP ()
{
    if (!GCP) return;

    cout << "Generating GCP configuration files" << endl;
    for (pair<string,pt::ptree> it: *GCP) {

        string name = it.first;
        cout << "GCP: " << name << endl;

        vector<int> IOVs = GetIOVsFromTree(it.second);
        bool multiIOV = IOVs.size() > 1;

        if (multiIOV) cout << "Several IOVs were found";

        for (int IOV: IOVs) {
            cout << "Preparing validation for IOV " << IOV << endl;
            it.second.put<int>("IOV", IOV);

            fs::path subdir = dir / fs::path("GCP") / name;
            if (multiIOV) subdir /= fs::path(to_string(IOV));
            cout << "The validation will be performed in " << subdir << endl;

            string jobname = "GCP_" + name;
            if (multiIOV) jobname +=  '_' + to_string(IOV);

            Job job(jobname, "GCP", subdir);

            string twig_ref  = it.second.get<string>("reference"),
                   twig_test = it.second.get<string>("test");

            job.tree.put<string>("LFS", LFS.string());
            job.tree.put_child(twig_ref, alignments.get_child(twig_ref));
            job.tree.put_child(twig_test, alignments.get_child(twig_test));
            job.tree.put_child(name, it.second);
            if (job.tree.count("IOVs")) job.tree.erase("IOVs");

            cout << "Queuing job" << endl;
            dag << job;
        }
    }

}

void Validation::PrepareDMR ()
{
    if (!DMR) return;

    boost::optional<pt::ptree&> singles = DMR->get_child_optional("single"),
                                merges  = DMR->get_child_optional("merge"),
                                trends  = DMR->get_child_optional("trend");

    // TODO: look up preexisting validation
    // TODO: split into subroutines

    map<string,vector<int>> DMRsingles; // key = name, value = IOV list
    if (singles) {
        cout << "Generating DMR configuration files" << endl;
        for (pair<string,pt::ptree> it: *singles) {

            string name = it.first;
            cout << "DMR: " << name << endl;

            DMRsingles[name] = GetIOVsFromTree(it.second);
            bool multiIOV = DMRsingles[name].size() > 1;

            for (int IOV: DMRsingles[name]) {
                it.second.put<int>("IOV", IOV);

                fs::path subdir = dir / fs::path("DMR/single") / name;
                if (multiIOV) subdir /= fs::path(to_string(IOV));
                cout << "The validation will be performed in " << subdir << endl;

                string jobname = "DMRsingle_" + name;
                if (multiIOV) jobname +=  '_' + to_string(IOV);

                Job job(jobname, "DMR", subdir);

                job.tree.put<string>("LFS", LFS.string());

                vector<string> current_alignments = it.second.get<vector<string>>("alignments", tok_str);
                for (string a: current_alignments) 
                    job.tree.put_child(a, alignments.get_child(a));
                job.tree.put_child(name, it.second);
                if (job.tree.count("IOVs")) job.tree.erase("IOVs");

                // TODO: look up the dataset
                // TODO: write the name of the outputs in alignments

                cout << "Queuing job" << endl;
                dag << job;
            }
        }
    }

    if (merges) {
        for (pair<string,pt::ptree> it: *merges) {

            string name = it.first;
            cout << name << endl;

            vector<string> singles = it.second.get<vector<string>>("single", tok_str);

            for (string& single: singles) {

                if (DMRsingles.find(single) == DMRsingles.end()) {
                    // TODO: look it up in the alignments
                    cerr << single << " could not be found\n";
                    exit(EXIT_FAILURE);
                }

                // TODO: IOVs?
            }
        }
    }

    if (trends) {
        // TODO
    }
}

void Validation::CloseDag ()
{
    cout << "Closing dag file" << endl;
    dag.close();
}

int Validation::SubmitDag () const
{
    cout << "Submitting" << endl;
    return system("echo condor_submit_dat "); // TODO: remove `echo`
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// The main function contains:
//  - the parsing of the arguments with Boost Program Options &
//  - most of the handling of the exceptions/errors
// while the real "main" function containing the essence of the program
// is split into the different methods of the `Validation` class.
int main (int argc, char * argv[])
{
    try {
        ValidationProgramOptions options;
        try {
            options.helper(argc, argv);
            options.parser(argc, argv);
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
            Validation validation(options.config);
            validation.PrepareGCP();
            validation.PrepareDMR();
            validation.CloseDag();

            if (options.dry) {
                cout << "Dry run, exiting now" << endl;
                return EXIT_SUCCESS;
            }

            return validation.SubmitDag();
        }
        catch (pt::ptree_bad_data & e) {
            cerr << "Property Tree Bad Data Error: " << e.data<string>() << '\n';
        }
        catch (pt::ptree_bad_path & e) {
            cerr << "Property Tree Bad Path Error: " << e.path<string>() << '\n';
        }
        catch (pt::ptree_error & e) {
            cerr << "Property Tree Error: " << e.what() << '\n';
        }
    }
    // general boost exception
    catch (boost::exception & e ) {
        cerr << "Boost exception: " << boost::diagnostic_information(e);
    }
    // standard exception
    //catch (bad_any_cast & e) {
    //    cerr << "Bad any cast:" << e.what() << '\n';
    //}
    catch (logic_error & e) {
        cerr << "Logic Error: " << e.what() << '\n';
    }
    catch (exception & e) {
        cerr << "Standard Error: " << e.what() << '\n';
    }
    catch (...) { 
        cerr << "Unkown failure\n";
    }
    return EXIT_FAILURE;
}
#endif
