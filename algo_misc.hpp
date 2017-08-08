#ifndef algo_misc_hpp
#define algo_misc_hpp

#include "util.hpp"
#include <functional>
#include "linalg_util.hpp"
#include "geometric.hpp"
#include "math_util.hpp"

using namespace avl;

template<typename T>
class VoxelArray
{
    int3 size;
    std::vector<T> voxels;
public:
    VoxelArray(const int3 & size) : size(size), voxels(size.x * size.y * size.z) {}
    const int3 & get_size() const { return size; }
    T operator[](const int3 & coords) const { return voxels[coords.z * size.x * size.y + coords.y * size.x + coords.x]; }
    T & operator[](const int3 & coords) { return voxels[coords.z * size.x * size.y + coords.y * size.x + coords.x]; }
};

class SuperFormula
{
    float m, n1, n2, n3, a, b;
public:
    SuperFormula(const float m, const float n1, const float n2, const float n3, const float a = 1.0f, const float b = 1.0f) : m(m), n1(n1), n2(n2), n3(n3), a(a), b(b) {}

    float operator() (const float phi) const
    {
        float raux1 = std::pow(std::abs(1.f / a * std::cos(m * phi / 4.f)), n2) + std::pow(std::abs(1.f / b * std::sin(m * phi / 4.f)), n3);
        float r1 = std::pow(std::abs(raux1), -1.0f / n1);
        return r1;
    }
};

// Cantor set on the xz plane
struct CantorSet
{
    std::vector<Line> lines{ {float3(-1, 0, 0), float3(1, 0, 0)} }; // initial

    std::vector<Line> next(const Line & line) const
    {
        const float3 p0 = line.origin;
        const float3 pn = line.direction;
        float3 p1 = (pn - p0) / 3.0f + p0;
        float3 p2 = ((pn - p0) * 2.f) / 3.f + p0;

        std::vector<Line> next;
        next.push_back({ p0, p1 });
        next.push_back({ p2, pn });

        return next;
    }

    void step()
    {
        std::vector<Line> recomputedSet;
        for (auto & line : lines)
        {
            std::vector<Line> nextLines = next(line); // split
            std::copy(nextLines.begin(), nextLines.end(), std::back_inserter(recomputedSet));
        }
        recomputedSet.swap(lines);
    }
};

struct SimpleHarmonicOscillator
{
    float frequency = 0, amplitude = 0, phase = 0;
    float value() const { return std::sin(phase) * amplitude; }
    void update(float timestep) { phase += frequency * timestep; }
};

inline std::vector<bool> make_euclidean_pattern(int steps, int pulses)
{
    std::vector<bool> pattern;

    std::function<void(int, int, std::vector<bool> &, std::vector<int> &, std::vector<int> &)> bjorklund;

    bjorklund = [&bjorklund](int level, int r, std::vector<bool> & pattern, std::vector<int> & counts, std::vector<int> & remainders)
    {
        r++;
        if (level > -1)
        {
            for (int i = 0; i < counts[level]; ++i) bjorklund(level - 1, r, pattern, counts, remainders);
            if (remainders[level] != 0) bjorklund(level - 2, r, pattern, counts, remainders);
        }
        else if (level == -1) pattern.push_back(false);
        else if (level == -2) pattern.push_back(true);
    };

    if (pulses > steps || pulses == 0 || steps == 0) return pattern;

    std::vector<int> counts;
    std::vector<int> remainders;

    int divisor = steps - pulses;
    remainders.push_back(pulses);
    int level = 0;

    while (true)
    {
        counts.push_back(divisor / remainders[level]);
        remainders.push_back(divisor % remainders[level]);
        divisor = remainders[level];
        level++;
        if (remainders[level] <= 1) break;
    }

    counts.push_back(divisor);

    bjorklund(level, 0, pattern, counts, remainders);

    return pattern;
}

inline float3 rgb_to_hsv(const float3 & rgb)
{
    float rd = rgb.x / 255;
    float gd = rgb.y / 255;
    float bd = rgb.z / 255;

    float max = ::max(rd, gd, bd), min = ::min(rd, gd, bd);
    float h, s, v = max;

    float d = max - min;
    s = max == 0 ? 0 : d / max;

    if (max == min)
    {
        h = 0; // achromatic
    }
    else
    {
        if (max == rd)  h = (gd - bd) / d + (gd < bd ? 6 : 0);
        else if (max == gd) h = (bd - rd) / d + 2;
        else if (max == bd) h = (rd - gd) / d + 4;
        h /= 6;
    }

    return float3(h, s, v);
}

inline float3 hsv_to_rgb(const float3 & hsv)
{
    float r, g, b;

    float h = hsv.x;
    float s = hsv.y;
    float v = hsv.z;

    int i = int(h * 6);

    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i % 6)
    {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }

    float3 rgb;
    rgb.x = uint8_t(clamp((float)r * 255.0f, 0.0f, 255.0f));
    rgb.y = uint8_t(clamp((float)g * 255.0f, 0.0f, 255.0f));
    rgb.z = uint8_t(clamp((float)b * 255.0f, 0.0f, 255.0f));
    return rgb;
}

inline float3 interpolate_color(const float3 & rgb_a, const float3 & rgb_b, const float t)
{
    float3 aHSV = rgb_to_hsv(rgb_a);
    float3 bHSV = rgb_to_hsv(rgb_b);
    float3 result = linalg::lerp(aHSV, bHSV, t);
    return hsv_to_rgb(result);
}

#endif // end algo_misc_hpp
