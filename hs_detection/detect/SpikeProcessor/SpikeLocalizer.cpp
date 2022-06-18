#include "SpikeLocalizer.h"
#include <utility>
#include <vector>
#include <algorithm>
#include "../Detection.h"
#include "../Point.h"

using namespace std;

namespace HSDetection
{
    void SpikeLocalizer::operator()(Spike *pSpike)
    {
        Point sumCoM(0, 0);
        int sumWeight = 0;

        int frameLeft = pSpike->frame - Detection::noise_duration;
        int frameRight = pSpike->frame + Detection::noise_duration;
        // TODO: assert (cutout_start >= noise_duration && cutout_end >= noise_duration)

        vector<pair<int, int>> chAmp;

        const short *baselines = Detection::QBs[pSpike->frame - Detection::spike_peak_duration]; // TODO: tSpike > peakDur?

        for (int i = 0; i < Detection::num_com_centers; i++)
        {
            chAmp.clear();

            // TODO: check i in inner neighbor range (numCoM not too large)
            int centerChannel = Detection::probeLayout.getInnerNeighbors(pSpike->channel)[i];

            for (int curr_neighbor_channel : Detection::probeLayout.getInnerNeighbors(centerChannel))
            {
                int offset = Detection::AGlobal(pSpike->frame, 0) + baselines[curr_neighbor_channel];
                int sum = 0;
                for (int t = frameLeft; t < frameRight; t++)
                {
                    int curr_amp = Detection::trace(t, curr_neighbor_channel) - offset;
                    if (curr_amp >= 0)
                    {
                        sum += curr_amp;
                    }
                }
                chAmp.push_back(make_pair(curr_neighbor_channel, sum));
            }

            int chCount = chAmp.size();

            int median;
            {
                // TODO: extract as a class?
                vector<pair<int, int>>::iterator mid = chAmp.begin() + (chCount - 1) / 2;
                nth_element(chAmp.begin(), mid, chAmp.end(),
                            [](const pair<int, int> &lhs, const pair<int, int> &rhs)
                            { return lhs.second < rhs.second; });
                median = mid->second;
                if (chCount % 2 == 0)
                {
                    int next = min_element(mid + 1, chAmp.end(),
                                           [](const pair<int, int> &lhs, const pair<int, int> &rhs)
                                           { return lhs.second < rhs.second; })
                                   ->second;
                    median = (median + next) / 2;
                }
            }

            Point CoM(0, 0);
            int sumAmp = 0;
            for (int i = 0; i < chCount; i++)
            {
                int amp = chAmp[i].second - median; // correction and threshold
                if (amp > 0)
                {
                    CoM += amp * Detection::probeLayout.getChannelPosition(chAmp[i].first);
                    sumAmp += amp;
                }
            }

            if (sumAmp == 0)
            {
                // NOTE: amp > 0 never entered, all <= median, i.e. max <= median
                // NOTE: this iff. max == median, i.e. upper half value all same
                // NOTE: unlikely happens, therefore loop again instead of merge into previous
                // NOTE: choose any point with amp == median == max
                // TODO: really need? choose any?
                vector<pair<int, int>>::iterator it = find_if(chAmp.begin(), chAmp.end(),
                                                              [median](const pair<int, int> &x)
                                                              { return x.second == median; });
                CoM = Detection::probeLayout.getChannelPosition(it->first);
            }
            else
            {
                CoM /= sumAmp;
            }

            sumCoM += 1 * CoM; // TODO: weight?
            sumWeight += 1;    // TODO: inc amount?
        }

        pSpike->position = sumCoM / sumWeight;
    }

} // namespace HSDetection
