// Copyright (c) 2022 Sultim Tsyrendashiev
//
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

#ifndef EXPOSURE_H_
#define EXPOSURE_H_

#ifndef DESC_SET_TONEMAPPING
    #error DESC_SET_TONEMAPPING is required
#endif

float getManualEV100( float aperture, float shutterTime, float iso )
{
    return log2( square( aperture ) / shutterTime * 100.0 / iso );
}

float getAutoEV100()
{
    const float lumAverage = max( 0.0, tonemapping.avgLuminance );
    const float S          = 100;
    const float K          = 12.5;
    return log2( lumAverage * S / K );
}

float getCurrentEV100()
{
    bool manual = false;
    return manual ? getManualEV100( 1.0 / 8.0, 1.0 / 500.0, 100 ) : getAutoEV100();
}

float ev100ToLuminousExposure( float ev100 )
{
    float maxLuminance = 1.2 * exp2( ev100 );
    return maxLuminance > 0.0 ? 1.0 / maxLuminance : 0.0;
}

float ev100ToLuminance( float ev100 )
{
    return exp2( ev100 - 3 );
}

#endif // EXPOSURE_H_