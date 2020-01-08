#include <string>
#include <vector>

#include <boost/property_tree/ptree.hpp>

#include <experimental/filesystem>

namespace pt = boost::property_tree;
namespace fs = std::experimental::filesystem;

////////////////////////////////////////////////////////////////////////////////
/// namespace AllInOneConfig
///
/// Configuration file for validation in context of tracker alignment
namespace AllInOneConfig {

////////////////////////////////////////////////////////////////////////////////
/// structure General
///
/// Corresponds to `[general]`, supposed to be unique in a INI config file.
struct General {
    std::string name, //!< name, used to identify the validation in the queues
                datadir, //!< output directory for the plots
                eosdir; //!< output directory for the root files

    General //!< Main constructor
        (pt::ptree tree); //!< Tree should correspond to a INI file (although it could be generalised)
    General //!< Copy constructor
        (const General& g);

    void Print () const; //!< Print the content of the structure
};

////////////////////////////////////////////////////////////////////////////////
/// structure Condition
///
/// Corresponds to `condition record = connection, tag, label`
struct Condition {
    std::string name, value;

    Condition (std::string Name, std::string Value);
    Condition (std::string Name, pt::ptree tree);
    Condition (const Condition& c);
};

////////////////////////////////////////////////////////////////////////////////
/// structure Validate
///
/// Corresponds to `validate Validation = Alignment1, Alignment2, ...`
struct Validate {
    std::string name, value;

    Validate (std::string Name, std::string Value);
    Validate (std::string Name, pt::ptree tree);
    Validate (const Validate& v);
};

////////////////////////////////////////////////////////////////////////////////
/// structure Alignment
///
/// Corresponds to `[alignment:...]`
struct Alignment {
    std::string name, //!< corresponds to `[alignment:name]`
                globaltag, //!< GT 
                path; //!< path to validation, if existing (non mandatory)
    int color, //!< root color code
        style; //!< root style code
    std::vector<Condition> conditions; //!< conditions to override (non mandatory)

    Alignment //!< Main constructor
        (std::string Name, pt::ptree tree);
    Alignment //!< Copy constructor
        (const Alignment& a);

    void Print () const; //!< Print the content of the structure
};

struct DMR {
    std::string name, //!< corresponds to `[DMR:name]`
                dataset, //!< (partial) name of the file containing the data
                trackcollection, //!< track collection, depending on dataset
                path; //!< path to directory if applicable
    int maxevents, //!< max number of events
        maxtracks, //!< max number of tracks
        maxhits, //!< max number of hits *per module*
        minhits; //!< min number of hits *per module*
    bool magneticfield; //!< true = on, false = off
    std::vector<int> IOVs; //!< list of IOVs, non mandatory

    DMR //!< Main constructor
        (std::string Name, pt::ptree tree);
    DMR //!< Copy constructor
        (const DMR& d);

    void Print () const; //!< Print the content of the structure
};

////////////////////////////////////////////////////////////////////////////////
/// structure Plot
///
/// Wrapper of block of the same name in the INI config.
/// It contains general cosmetic details and indicates with validation will be 
/// present on the figure.
struct Plot {
    const std::string name, //!< corresponds to `[plot:name]`
                      legendoptions, //!< legend options, exact meaning may differ according to the validation
                      title, //!< plot title, exact meaning may differ according to the validation
                      curves; //!< TODO
    const bool usefit; //!< TODO
    const std::vector<Validate> validates; //!< ex: `validate myDMR = align1, align2` will run the block [DMR:myDMR] for [alignment:align1] and [alignment:align2]

    Plot //!< Main constructor
        (std::string Name, pt::ptree tree);
    Plot //!< Copy constructor
        (const Plot& p); 

    void Print () const; //!< Print the content of the structure
};

////////////////////////////////////////////////////////////////////////////////
/// structure Config
///
/// Basically a wrapper of the INI config file into a structure (of structures).
/// Its functionalities are minimal: just store, print, write and give access.
///
/// *NB*: the INI format is slightly extended, where the colon (':') is used as 
/// separator
struct Config {

    const General general; //!< Containing generalities such as input and output directories
    const std::vector<Alignment> alignments; //!< Details about the MillePede or HipPy objects
    const std::vector<DMR> dmrs; //!< Configuration of the DMR validation
    // TODO: add other types of validation here, such as PV, etc.
    const std::vector<Plot> plots; //!< An "instance" of the validation

    Config //!< Main constructor
        (pt::ptree tree); //!< corresponds (almost) to INI format
    Config //!< Copy constructor
        (const Config& c);

    void Print () const; //!< Print the content of the structure
};

}
