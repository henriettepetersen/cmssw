#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

#include "AllInOneConfig.h"

#include "ColorParser.h"

using namespace AllInOneConfig;
using namespace std;

/****************************** local tools ******************************/

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// used in two contexts:
// 1) to distinguish different blocks having the same structure: `[foo:*]`
// 2) to distinguish different options having the same type: `foo bar = value`
template<typename T> vector<T> GetChildren (const string prefix, const pt::ptree& tree) 
{
    vector<T> vec;
    const int length = prefix.length();
    for (const pt::ptree::value_type & el: tree) {
        auto position = el.first.find(prefix);
        if (position != string::npos) {
            string name = el.first.substr(position + length);
            T t(name, el.second);
            vec.push_back(t);
        }
    }
    return vec;
}

// just a wrapper to get the IOVs from a comma-separated string into a vector of integers
vector<int> GetIOVs (string s)
{
    boost::char_separator<char> sep{","};
    boost::tokenizer<boost::char_separator<char>> tok{s,sep};

    vector<int> IOVs; 
    for (const auto& t: tok) {
        int IOV = stoi(boost::trim_copy(t));
        IOVs.push_back(IOV);
    }
    return IOVs;
}
#endif

/****************************** General ******************************/

General::General (pt::ptree tree) :
    name(tree.get<string>("name")),
    datadir(tree.get<string>("datadir")),
    eosdir(tree.get<string>("eosdir"))
{ }

General::General (const General& c) :
    name(c.name),
    datadir(c.datadir),
    eosdir(c.eosdir)
{ }

void General::Print () const
{
    cout << "\nGeneral: name = " << name
         << "\nGeneral: datadir = " << datadir
         << "\nGeneral: eosdir = " << eosdir
         << endl;
}

/****************************** Condition ******************************/

Condition::Condition (string Name, string Value) :
    name(Name), value(Value)
{ }

Condition::Condition (string Name, pt::ptree tree) :
    name(Name), value(tree.data())
{ }

Condition::Condition (const Condition& c) :
    name(c.name), value(c.value)
{ }

/****************************** Validate ******************************/

Validate::Validate (string Name, string Value) :
    name(Name), value(Value)
{ }

Validate::Validate (string Name, pt::ptree tree) :
    name(Name), value(tree.data())
{ }

Validate::Validate (const Validate& v) :
    name(v.name), value(v.value)
{ }


/****************************** Alignment ******************************/

Alignment::Alignment (string Name, pt::ptree tree) :
    name(Name),
    globaltag(tree.get<string>("globaltag")),
    path(tree.get("path", string())),
    color(String2Color(tree.get<string>("color"))),
    style(tree.get<int>("style")),
    conditions(GetChildren<Condition>("condition ", tree))
{ }

Alignment::Alignment (const Alignment& a) :
    name(a.name),
    globaltag(a.globaltag),
    path(a.path),
    color(a.color), style(a.style),
    conditions(a.conditions)
{ }

void Alignment::Print () const
{
    cout << "\nAlignment: name = " << name
        << "\nAlignment: globaltag = " << globaltag
        << "\nAlignment: color = " << color
        << "\nAlignment: style = " << style
        << "\nAlignment: path = " << path;
    for (const Condition& c: conditions)
        cout << "\nAlignment: condition " << c.name << " = " << c.value;
    cout << endl;
}

/****************************** DMR ******************************/

DMR::DMR (string Name, pt::ptree tree) :
    name(Name),
    dataset(tree.get<string>("dataset")),
    trackcollection(tree.get<string>("trackcollection")),
    path(tree.get("path", string())),
    maxevents(tree.get("maxevents", -1)),
    maxtracks(tree.get("maxtracks", -1)),
    maxhits(tree.get("maxhits", -1)),
    minhits(tree.get("minhits", 15)),
    magneticfield(tree.get("magneticfield", true)),
    IOVs(GetIOVs(tree.get("IOVs", string())))
{ }

DMR::DMR (const DMR& d) :
    name(d.name),
    dataset(d.dataset),
    trackcollection(d.trackcollection),
    path(d.path),
    maxevents(d.maxevents),
    maxtracks(d.maxtracks),
    maxhits(d.maxhits),
    minhits(d.minhits),
    magneticfield(d.magneticfield),
    IOVs(d.IOVs)
{ }

void DMR::Print () const
{
    cout << "\nDMR: name = "            << name
         << "\nDMR: dataset = "         << dataset
         << "\nDMR: trackcollection = " << trackcollection
         << "\nDMR: path = "            << path
         << "\nDMR: maxevents = "       << maxevents
         << "\nDMR: maxtracks = "       << maxtracks
         << "\nDMR: maxhits = "         << maxhits
         << "\nDMR: minhits = "         << minhits
         << "\nDMR: magneticfield = "   << magneticfield
         << "\nDMR: IOVs = ";
    for (const int& IOV: IOVs)
        cout << IOV << ' ';
    cout << endl;
}

/****************************** Plot ******************************/

Plot::Plot (string Name, pt::ptree tree) :
    name(Name),
    legendoptions(tree.get<string>("legendoptions")),
    title(tree.get("title", string())),
    curves(tree.get<string>("curves")), // TODO: tokenise?
    usefit(tree.get<bool>("usefit")),
    validates(GetChildren<Validate>("validate ", tree))
{ }

Plot::Plot (const Plot& p) :
    name(p.name),
    legendoptions(p.legendoptions),
    title(p.title),
    curves(p.curves),
    usefit(p.usefit),
    validates(p.validates)
{ }

void Plot::Print () const
{
    cout << "\nPlot: name = "          << name
         << "\nPlot: legendoptions = " << legendoptions
         << "\nPlot: title = "         << title
         << "\nPlot: curves = "        << curves
         << "\nPlot: usefit = "        << usefit;
    for (const Validate& v: validates)
        cout << "\nPlot: validate " << v.name << " = " << v.value;
    cout << endl;
}

/****************************** Config ******************************/

Config::Config (pt::ptree tree) :
    general(tree.get_child("general")),
    alignments(GetChildren<Alignment>("alignment:", tree)),
    dmrs(GetChildren<DMR>("DMR:", tree)),
    plots(GetChildren<Plot>("plot:", tree))
{ }

Config::Config (const Config& c) :
    general(c.general),
    alignments(c.alignments),
    dmrs(c.dmrs),
    plots(c.plots)
{ }

void Config::Print () const
{
    general.Print();
    for (const Alignment& alignment: alignments)
        alignment.Print();
    for (const DMR& dmr: dmrs)
        dmr.Print();
    for (const Plot& plot: plots)
        plot.Print();
}
