#include <TString.h>
#include <TColor.h>

#include <cstdlib>
#include <iostream>
#include <exception>
#include <map>

Color_t String2Color (TString input)
{
    // 1) remove all space
    input.ReplaceAll(" ", "");

    // 2) if number, then just return it
    if (input.IsDec()) 
        return input.Atoi();

    // 3) if the first char is not a k, then crash
    if (input(0) != 'k')
        exit(EXIT_FAILURE);
   
    // 4) in case of + or -, needs to split the string
    Ssiz_t Plus  = input.First('+'),
           Minus = input.First('-');

    // 5) only one symbol is allowed
    if (Plus != TString::kNPOS && Minus != TString::kNPOS)
        exit(EXIT_FAILURE);

    static const std::map<TString, Color_t> colormap
    {
        {"kWhite"   ,  kWhite  },
        {"kBlack"   ,  kBlack  },
        {"kGrey"    ,  kGray   },
        {"kGray"    ,  kGray   },
        {"kRed"     ,  kRed    },
        {"kGreen"   ,  kGreen  },
        {"kBlue"    ,  kBlue   },
        {"kYellow"  ,  kYellow },
        {"kMagenta" ,  kMagenta},
        {"kCyan"    ,  kCyan   },
        {"kOrange"  ,  kOrange },
        {"kSpring"  ,  kSpring },
        {"kTeal"    ,  kTeal   },
        {"kAzure"   ,  kAzure  },
        {"kViolet"  ,  kViolet },
        {"kPink"    ,  kPink   }
    };

    // 6) treat the three cases: +, - or nothing
    try {
        if (Plus != TString::kNPOS) {
            TString Left = input(0, Plus),
                    Right = input(Plus+1);
            return colormap.at(Left) + Right.Atoi();
        }
        else if (Minus != TString::kNPOS) {
            TString Left = input(0, Minus),
                    Right = input(Minus+1);
            return colormap.at(Left) - Right.Atoi();
        }
        else {
            return colormap.at(input);
        }
    }
    catch (const std::out_of_range& oor) {
        std::cerr << "The color " << input << " does not exist"  << std::endl;
        exit(EXIT_FAILURE);
    }
}
