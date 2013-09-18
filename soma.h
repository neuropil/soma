/// @file soma.h
/// @brief soma software for Leap controller
/// @author Jeff Perry <jeffsp@gmail.com>
/// @version 1.0
/// @date 2013-08-26

#ifndef SOMA_H
#define SOMA_H

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <map>
#include <unistd.h>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Leap.h"

namespace soma
{

class frame_counter
{
    private:
    uint64_t frames;
    uint64_t first_ts;
    uint64_t last_ts;
    public:
    frame_counter ()
        : frames (0)
        , first_ts (0)
        , last_ts (0)
    {
    }
    void update (uint64_t ts)
    {
        if (frames == 0)
            first_ts = ts;
        else
            last_ts = ts;
        ++frames;
    }
    uint64_t get_frames () const
    {
        return frames;
    }
    double fps () const
    {
        double secs = static_cast<double> (last_ts - first_ts) / 1000000;
        // don't count the last frame
        if (frames != 0 && secs != 0.0f)
            return (frames - 1) / secs;
        return -1;
    }
};

template<typename T>
class sliding_time_window
{
    private:
    const uint64_t duration;
    std::deque<uint64_t> timestamps;
    std::deque<T> samples;
    void update (uint64_t ts)
    {
        while (!timestamps.empty ())
        {
            assert (samples.size () == timestamps.size ());
            assert (ts >= timestamps.back ());
            if (ts - timestamps.back () > duration)
            {
                timestamps.pop_back ();
                samples.pop_back ();
            }
            else
                break;
        }
    }
    public:
    sliding_time_window (uint64_t duration)
        : duration (duration)
    {
    }
    void clear ()
    {
        samples.clear ();
        timestamps.clear ();
    }
    double fullness (uint64_t ts) const
    {
        if (timestamps.empty ())
            return 0.0f;
        double start = timestamps.back ();
        assert (start <= ts);
        assert (duration != 0);
        return (ts - start) / duration;
    }
    void add_sample (uint64_t ts, const T &n)
    {
        // remove samples with old timestamps
        update (ts);
        // don't add the same sample twice
        assert (timestamps.empty () || timestamps.front () != ts);
        timestamps.emplace_front (ts);
        samples.emplace_front (n);
    }
    const std::deque<T> get_samples () const
    {
        return samples;
    }
    const std::deque<uint64_t> get_timestamps () const
    {
        return timestamps;
    }
    void dump (std::ostream &s) const
    {
        uint64_t start = samples.front ().first;
        for (auto i : samples)
            s << ' ' << '<' << start - i.first << ',' << i.second << '>';
        s << std::endl;
    }
};

bool sort_left_to_right (const Leap::Pointable &a, const Leap::Pointable &b)
{
    return a.tipPosition ().x < b.tipPosition ().x;
}

const size_t FVN =
      5 * 3 // 5 tip velocities
    + 5 * 3 // 5 tip directions
    + 4 * 1 // 4 between distances
    + 4 * 3; // 4 between directions

struct finger
{
    Leap::Vector position;
    Leap::Vector velocity;
    Leap::Vector direction;
};

Leap::Vector noisy_default_position ()
{
    Leap::Vector v;
    v.x = (rand () * 2.0) / RAND_MAX - 1.0;
    v.y = (rand () * 2.0) / RAND_MAX - 1.0;
    v.z = (rand () * 2.0) / RAND_MAX - 1.0;
    return v;
}

Leap::Vector noisy_default_velocity ()
{
    Leap::Vector v = noisy_default_position ();
    return v;
}

Leap::Vector noisy_default_direction ()
{
    Leap::Vector v = noisy_default_position ();
    // make it point down
    v.y += -100;
    return v.normalized ();
}

class hand : public std::array<finger, 5>
{
    public:
    hand (const Leap::PointableList &pl)
    {
        // only use at most 5 pointables
        std::vector<Leap::Pointable> p (std::min (pl.count (), 5));
        // get vector of pointables.  unfortunately, the [] operator returns a copy of the pointable, not a reference,
        // so we are forced to sort copies of the objects rather than sorting pointers to the objects.
        for (size_t i = 0; i < p.size (); ++i)
            p[i] = pl[i];
        // sort
        sort (p.begin (), p.end (), sort_left_to_right);
        // a hand always has five fingers
        // if a finger was not detected, fill it with some default values
        for (size_t i = 0; i < 5; ++i)
        {
            if (i < p.size ())
            {
                data ()[i].position = p[i].tipPosition ();
                data ()[i].velocity = p[i].tipVelocity ();
                data ()[i].direction = p[i].direction ();
            }
            else
            {
                data ()[i].position = noisy_default_position ();
                data ()[i].velocity = noisy_default_velocity ();
                data ()[i].direction = noisy_default_direction ();
            }
        }
    }
};

class feature_vector : public std::array<double,FVN>
{
    public:
    feature_vector ()
    {
    }
    feature_vector (const hand &h)
    {
        // stuff them into the vector
        size_t i = 0;
        for (size_t j = 0; j < 5; ++j)
        {
            assert (i < size ());
            assert (j < h.size ());
            data ()[i++] = h[j].velocity.x;
            data ()[i++] = h[j].velocity.y;
            data ()[i++] = h[j].velocity.z;
        }
        for (size_t j = 0; j < 5; ++j)
        {
            assert (i < size ());
            assert (j < h.size ());
            data ()[i++] = h[j].direction.x;
            data ()[i++] = h[j].direction.y;
            data ()[i++] = h[j].direction.z;
        }
        for (size_t j = 0; j < 4; ++j)
        {
            assert (i < size ());
            assert (j + 1 < h.size ());
            float d = h[j].position.distanceTo (h[j + 1].position);
            data ()[i++] = d;
        }
        for (size_t j = 0; j < 4; ++j)
        {
            assert (i < size ());
            assert (j + 1 < h.size ());
            Leap::Vector dir = h[j].position - h[j + 1].position;
            data ()[i++] = dir.x;
            data ()[i++] = dir.y;
            data ()[i++] = dir.z;
        }
        assert (i == size ());
    }
};

feature_vector zero_movement (const feature_vector &f)
{
    feature_vector z (f);
    size_t i = 0;
    for (size_t j = 0; j < 5; ++j)
    {
        z[i++] = 0.0;
        z[i++] = 0.0;
        z[i++] = 0.0;
    }
    return z;
}

typedef std::vector<uint64_t> timestamps;
typedef std::vector<feature_vector> feature_vectors;

enum class hand_position
{
    unknown,
    pointing,
    clicking,
    scrolling,
    centering
};

std::string to_string (const hand_position hp)
{
    switch (hp)
    {
        default:
        throw std::runtime_error ("invalid hand position");
        case hand_position::unknown:
        return std::string ("unknown");
        case hand_position::pointing:
        return std::string ("pointing");
        case hand_position::clicking:
        return std::string ("clicking");
        case hand_position::scrolling:
        return std::string ("scrolling");
        case hand_position::centering:
        return std::string ("centering");
    }
}

template<typename T,size_t N>
class stats
{
    private:
    size_t total;
    std::array<T,N> u1;
    std::array<T,N> u2;
    public:
    template<typename A>
    void update (const A &v)
    {
        assert (v.size () == u1.size ());
        assert (v.size () == u2.size ());
        ++total;
        for (size_t i = 0; i < v.size (); ++i)
        {
            u1[i] += v[i];
            u2[i] += v[i] * v[i];
        }
    }
    double mean (size_t i) const
    {
        return static_cast<double> (u1[i]) / total;
    }
    double variance (size_t i) const
    {
        double u = mean (i);
        assert (static_cast<double> (u2[i]) / total >= u * u);
        return static_cast<double> (u2[i]) / total - u * u;
    }
};

class hand_position_classifier
{
    private:
    std::map<hand_position,stats<double,FVN>> mhps;
    public:
    hand_position_classifier ()
    {
    }
    void update (const hand_position hp, const feature_vectors &fvs)
    {
        for (auto i : fvs)
            mhps[hp].update (zero_movement (i));
        for (auto s : mhps)
        {
            std::clog << to_string (s.first) << std::endl;
            for (size_t j = 0; j < FVN; ++j)
            {
                double m = s.second.mean (j); // mean of dimension's dist
                double v = s.second.variance (j); // var of dimension's dist
                std::clog << j << ' ' << m << ' ' << sqrt (v) << std::endl;
            }
        }
    }
    void classify (const feature_vectors &fvs, const timestamps &ts, hand_position &hp, double &p) const
    {
        std::map<hand_position,double> l;
        for (auto h : {
            hand_position::pointing,
            hand_position::clicking,
            hand_position::scrolling,
            hand_position::centering })
        {
            for (auto i : fvs)
            {
                auto z = zero_movement (i);
                for (size_t j = 0; j < z.size (); ++j)
                {
                    // get p for this feature vector dimension
                    double x = z[j]; // feature dimension value
                    auto s = mhps.find (h);
                    assert (s != mhps.end ());
                    double m = s->second.mean (j); // mean of dimension's dist
                    double v = s->second.variance (j); // var of dimension's dist
                    // update log likelihood
                    if (v != 0.0f)
                        l[h] -= (x - m) * (x - m) / (2 * v);
                }
            }
        }
        double best_value = std::numeric_limits<int>::min ();
        hand_position best_hp = hand_position::unknown;
        for (auto i : l)
        {
            std::clog
                << to_string (i.first)
                << " = "
                << i.second
                << std::endl;
            if (i.second > best_value)
            {
                best_hp = i.first;
                best_value = i.second;
            }
        }
        hp = best_hp;
        p = best_value;
    }
};

enum class hand_movement
{
    unknown,
    still,
    moving,
    clicking,
    right_clicking
};

class hand_movement_classifier
{
    private:
    static const uint64_t FEATURE_WINDOW_DURATION = 500000;
    sliding_time_window<feature_vector> w;
    public:
    hand_movement_classifier ()
        : w (FEATURE_WINDOW_DURATION)
    {
    }
};

}

#endif
