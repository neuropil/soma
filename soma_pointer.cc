/// @file soma_pointer.cc
/// @brief soma pointer leap interface
/// @author Jeff Perry <jeffsp@gmail.com>
/// @version 1.0
/// @date 2013-08-30

#include "audio.h"
#include "soma.h"
#include "options.h"
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#elif defined(__APPLE__) && defined(__MACH__)
#elif defined(_MSC_VER)
#else
#error("unknown OS")
#endif

using namespace std;
using namespace soma;
using namespace Leap;
const string usage = "usage: soma_pointer";

class mouse
{
    private:
    Display *d;
    public:
    mouse ()
        : d (XOpenDisplay (0))
    {
        if (!d)
            throw runtime_error ("Could not open X display");
    }
    ~mouse ()
    {
        XCloseDisplay (d);
    }
    void click (int button, Bool down)
    {
        XTestFakeButtonEvent (d, button, down, CurrentTime);
        XFlush (d);
    }
    void move (int x, int y)
    {
        XWarpPointer (d, None, None, 0, 0, 0, 0, x, y);
        XFlush (d);
    }
};

class pinch_1d_control
{
    private:
    sliding_time_window<uint64_t> w;
    float min;
    float max;
    float d;
    public:
    pinch_1d_control (uint64_t duration, float min = 30, float max = 100)
        : w (duration)
        , min (min)
        , max (max)
        , d (0.0f)
    {
    }
    void update (uint64_t ts, const Leap::PointableList &p)
    {
        w.update (ts);
        if (p.count () != 2 || !p[0].isValid () || !p[1].isValid ())
            return;
        float x = p[0].tipPosition ().distanceTo (p[1].tipPosition ());
        w.add_sample (ts, x);
        if (w.full (85, ts))
            d = average (w.get_samples ());
    }
    float get_min () const
    {
        return min;
    }
    float get_max () const
    {
        return max;
    }
    float get_distance () const
    {
        return d < min ? min : (d > max ? max : d);
    }
};

class index_pointer
{
    private:
    sliding_time_window<Vector> w;
    float scale;
    mouse m;
    template<typename T>
    void move (const T &s)
    {
        if (s.size () < 2)
            return;
        auto a = s[s.size () - 1];
        auto b = s[s.size () - 2];
        auto x = a.x - b.x;
        auto y = a.y - b.y;
        auto z = a.z - b.z;
        if (z > x && z > y && z > 1)
            std::clog << "Click" << std::endl;
        else
            m.move (scale * -x, scale * y);
    }
    public:
    index_pointer (uint64_t position_window_duration,
        float scale = 2.0)
        : w (position_window_duration)
        , scale (scale)
    {
    }
    float get_scale () const
    {
        return scale;
    }
    void set_scale (float s)
    {
        scale = s;
    }
    void update (uint64_t ts, const Leap::PointableList &p)
    {
        w.update (ts);
        if (p.count () != 1 || !p[0].isValid ())
            return;
        w.add_sample (ts, p[0].tipPosition ());
        if (w.full (85, ts))
        {
            auto s = w.get_samples ();
            auto d = distances (s);
            if (average (d) > 1.0f)
                move (s);
        }
    }
};

class soma_pointer : public Listener
{
    private:
    bool done;
    frame_counter frc;
    finger_counter fic;
    pinch_1d_control p1d;
    index_pointer ip;
    bool sound_flag;
    audio au;
    public:
    soma_pointer (uint64_t finger_counter_window_duration,
        uint64_t finger_1d_control_window_duration,
        uint64_t position_window_duration,
        bool sound_flag)
        : done (false)
        , fic (finger_counter_window_duration)
        , p1d (finger_1d_control_window_duration)
        , ip (position_window_duration)
        , sound_flag (sound_flag)
    {
    }
    ~soma_pointer ()
    {
        clog << frc.fps () << "fps" << endl;
    }
    bool is_done () const
    {
        return done;
    }
    virtual void onInit (const Controller&)
    {
        clog << "onInit()" << endl;
    }
    virtual void onConnect (const Controller&)
    {
        clog << "onConnect()" << endl;
    }
    virtual void onDisconnect (const Controller&)
    {
        clog << "onDisconnect()" << endl;
    }
    virtual void onFrame(const Controller& c)
    {
        const Frame &f = c.frame ();
        frc.update (f.timestamp ());
        fic.update (f.timestamp (), f.pointables ());
        if (fic.is_changed ())
            clog << " fingers " << fic.count () << endl;
        if (fic.count () == 5)
        {
            if (sound_flag && !done)
                au.play (131, 100);
            done = true;
        }
        ip.update (f.timestamp (), f.pointables ());
        p1d.update (f.timestamp (), f.pointables ());
    }
};

const uint64_t FINGER_COUNTER_WINDOW_DURATION = 100000;
const uint64_t PINCH_1D_CONTROL_WINDOW_DURATION = 200000;
const uint64_t POSITION_WINDOW_DURATION = 200000;

int main (int argc, char **argv)
{
    try
    {
        if (argc != 1)
            throw runtime_error (usage);

        // options get saved here
        string config_fn = get_config_dir () + "/somarc";

        // configurable options
        options opts;

        // if the config file does not exist, write one
        {
            ifstream ifs (config_fn.c_str ());
            if (!ifs)
                write (opts, config_fn);
        }

        // read in the config file
        {
            ifstream ifs (config_fn.c_str ());
            if (!ifs) // if it's not there, notify the user
                clog << "warning: could not read configuration options" << endl;
            else
                read (opts, config_fn);
        }
        soma_pointer fp (
            FINGER_COUNTER_WINDOW_DURATION,
            PINCH_1D_CONTROL_WINDOW_DURATION,
            POSITION_WINDOW_DURATION,
            opts.get_sound ());
        Controller c (fp);

        // set to receive frames in the background
        c.setPolicyFlags (Controller::POLICY_BACKGROUND_FRAMES);

        clog << "press control-C to quit" << endl;

        while (!fp.is_done ())
        {
        }

        return 0;
    }
    catch (const exception &e)
    {
        cerr << e.what () << endl;
        return -1;
    }
}
