// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "HaltonSequence.h"


constexpr int JITTER_PHASE_COUNT = 128;

static float halton2[JITTER_PHASE_COUNT];
static float halton3[JITTER_PHASE_COUNT];

static bool isInitialized = false;


// Ray Tracing Gems II
// 3.2.3 SUPERSAMPLING
static void GenerateHaltonSequence(int b, float sequence[JITTER_PHASE_COUNT])
{
    int n = 0, d = 1;

    for (int i = 0; i < JITTER_PHASE_COUNT; ++i)
    {
        int x = d - n;
        if (x == 1)
        {
            n = 1;
            d *= b;
        }
        else
        {
            int y = d / b;
            while (x <= y)
            {
                y /= b;
            }
            n = (b + 1) * y - x;
        }
        sequence[i] = (float)n / (float)d;
    }
}


static void Initialize()
{
    GenerateHaltonSequence(2, halton2);
    GenerateHaltonSequence(3, halton3);

    isInitialized = true;
}


RgFloat2D RTGL1::HaltonSequence::GetJitter_Halton23(uint32_t frameId)
{
    if (!isInitialized)
    {
        Initialize();
    }

    return {
        halton2[frameId % JITTER_PHASE_COUNT] - 0.5f,
        halton3[frameId % JITTER_PHASE_COUNT] - 0.5f
    };
}
