/// @file classify.cc
/// @brief classify
/// @author Jeff Perry <jeffsp@gmail.com>
/// @version 1.0
/// @date 2013-09-20

#include <stdexcept>
#include "soma.h"

using namespace std;
using namespace soma;
const string usage = "usage: classify";

void classify (hand_sample_grabber &g, const hand_shape_classifier &hsc)
{
    const uint64_t SAMPLE_DURATION = 100000;
    while (1)
    {
        // get some frames
        g.grab (SAMPLE_DURATION);
        hand_samples s = g.get_hand_samples ();
        assert (!s.empty ());
        // filter out bad samples
        hand_samples fs = filter (s);
        // end if you show 6 or more fingers
        if (!fs.empty () && fs[0].size () > 5)
            break;
        //const size_t nf = s.size () - fs.size ();
        //clog << "filtered out " << nf << " samples" << endl;
        // convert them to feature vectors
        vector<hand_shape_features> fv (fs.begin (), fs.end ());
        // classify them
        unordered_map<hand_shape,double> l;
        hand_shape best_hs = hsc.classify (fv, l);
        static hand_shape last_hs = -1;
        if (last_hs != best_hs)
        {
            // show the class you got and its likelihood
            clog << "class = " << best_hs << endl;
            for (auto i : l)
                clog << i.first << ' ' << i.second << endl;
            last_hs = best_hs;
        }
    }
}

int main (int argc, char **argv)
{
    try
    {
        if (argc != 1)
            throw runtime_error (usage);

        hand_sample_grabber g;
        Leap::Controller c (g);
        hand_shape_classifier hsc;

        clog << "reading classifier from stdin" << endl;
        cin >> hsc;

        classify (g, hsc);

        return 0;
    }
    catch (const exception &e)
    {
        cerr << e.what () << endl;
        return -1;
    }
}
