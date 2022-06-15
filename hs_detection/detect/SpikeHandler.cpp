#include "SpikeHandler.h"
#include <deque>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include "ProcessSpikes.h"
#include "Detection.h"
#include "FilterSpikes.h"

using namespace std;

namespace SpikeHandler
{

    struct CustomLessThan
    {
        bool operator()(pair<int, float> const &lhs, pair<int, float> const &rhs) const
        {
            return lhs.second < rhs.second;
        }
    };

    void fillNeighborLayerMatrices()
    {
        int curr_channel;
        int curr_neighbor;
        float curr_dist;
        vector<pair<int, float>> distances_neighbors;
        vector<int> inner_neighbors;
        for (int i = 0; i < HSDetection::Detection::num_channels; i++)
        {
            curr_channel = i;
            for (int j = 0; j < HSDetection::Detection::max_neighbors; j++)
            {
                curr_neighbor = HSDetection::Detection::neighbor_matrix[curr_channel][j];
                if (curr_channel != curr_neighbor && curr_neighbor != -1)
                {
                    curr_dist = FilterSpikes::channelsDist(curr_neighbor, curr_channel);
                    distances_neighbors.push_back(make_pair(curr_neighbor, curr_dist));
                }
            }
            if (distances_neighbors.size() != 0)
            {
                sort(begin(distances_neighbors), end(distances_neighbors),
                     CustomLessThan());
                inner_neighbors = getInnerNeighborsRadius(distances_neighbors, i);
            }

            vector<int>::iterator it;
            it = inner_neighbors.begin();
            int curr_inner_neighbor;
            int k = 0;
            // Fill Inner neighbors matrix
            while (it != inner_neighbors.end())
            {
                curr_inner_neighbor = *it;
                HSDetection::Detection::inner_neighbor_matrix[i][k] = curr_inner_neighbor;
                ++k;
                ++it;
            }
            while (k < HSDetection::Detection::max_neighbors)
            {
                HSDetection::Detection::inner_neighbor_matrix[i][k] = -1;
                ++k;
            }
            // Fill outer neighbor matrix
            k = 0;
            for (int l = 0; l < HSDetection::Detection::max_neighbors; l++)
            {
                curr_neighbor = HSDetection::Detection::neighbor_matrix[i][l];
                if (curr_neighbor != -1 && curr_neighbor != i)
                {
                    bool is_outer_neighbor = true;
                    for (size_t m = 0; m < inner_neighbors.size(); m++)
                    {
                        if (HSDetection::Detection::inner_neighbor_matrix[i][m] == curr_neighbor)
                        {
                            is_outer_neighbor = false;
                            break;
                        }
                    }
                    if (is_outer_neighbor)
                    {
                        HSDetection::Detection::outer_neighbor_matrix[i][k] = curr_neighbor;
                        ++k;
                    }
                }
            }
            while (k < HSDetection::Detection::max_neighbors)
            {
                HSDetection::Detection::outer_neighbor_matrix[i][k] = -1;
                ++k;
            }
            inner_neighbors.clear();
            distances_neighbors.clear();
        }
    }

    vector<int>
    getInnerNeighborsRadius(vector<pair<int, float>> distances_neighbors,
                            int central_channel)
    {
        int curr_neighbor;
        float curr_dist;
        vector<int> inner_channels;
        vector<pair<int, float>>::iterator it;
        inner_channels.push_back(central_channel);
        it = distances_neighbors.begin();
        while (it != distances_neighbors.end())
        {
            curr_neighbor = it->first;
            curr_dist = it->second;
            if (curr_dist <= HSDetection::Detection::inner_radius)
            {
                inner_channels.push_back(curr_neighbor);
                ++it;
            }
            else
            {
                break;
            }
        }
        return inner_channels;
    }

    int **createInnerNeighborMatrix()
    {
        int **inner_neighbor_matrix;

        inner_neighbor_matrix = new int *[HSDetection::Detection::num_channels];
        for (int i = 0; i < HSDetection::Detection::num_channels; i++)
        {
            inner_neighbor_matrix[i] = new int[HSDetection::Detection::max_neighbors];
        }

        return inner_neighbor_matrix;
    }

    int **createOuterNeighborMatrix()
    {
        int **outer_neighbor_matrix;

        outer_neighbor_matrix = new int *[HSDetection::Detection::num_channels];
        for (int i = 0; i < HSDetection::Detection::num_channels; i++)
        {
            outer_neighbor_matrix[i] = new int[HSDetection::Detection::max_neighbors];
        }
        return outer_neighbor_matrix;
    }

    Spike storeWaveformCutout(int cutout_size, Spike curr_spike)
    {
        /*Stores the largest waveform in the written_cutout deque of the spike

        Parameters
        ----------
        channel: int
                The channel at which the spike is detected.
        cutout_size: int
                The length of the cutout
        curr_spike: Spike
                The current detected spike

        Returns
        ----------
        curr_spike: Spike
                The current detected spike (now with the waveform stored)
        */
        int32_t curr_written_reading;
        for (int i = 0; i < cutout_size; i++)
        {
            try
            {
                curr_written_reading = HSDetection::Detection::trace.get(
                    curr_spike.frame - HSDetection::Detection::cutout_start + i,
                    curr_spike.channel);
            }
            catch (...)
            {
                // spikes_filtered_file.close();
                cerr << "Raw Data and it parameters entered incorrectly, could not "
                        "access data. Terminating SpikeHandler."
                     << endl;
                exit(EXIT_FAILURE);
            }
            curr_spike.written_cutout.push_back(curr_written_reading);
        }
        return curr_spike;
    }

    Spike storeCOMWaveformsCounts(Spike curr_spike)
    {
        /*Stores the inner neighbor waveforms in the amp_cutouts deque of the spike

        Parameters
        ----------
        channel: int
                The channel at which the spike is detected.
        curr_spike: Spike
                The current detected spike

        Returns
        ----------
        curr_spike: Spike
                The current detected spike (now with the nearest waveforms stored)
        */

        vector<vector<pair<int, int>>> com_cutouts(HSDetection::Detection::num_com_centers, vector<pair<int, int>>());

        // Get closest channels for COM
        int channel = curr_spike.channel;
        for (int i = 0; i < HSDetection::Detection::num_com_centers; i++)
        {
            int curr_max_channel = HSDetection::Detection::inner_neighbor_matrix[channel][i];
            if (curr_max_channel == -1)
            {
                cerr << "num_com_centers too large. Not enough inner neighbors."
                     << endl;
                exit(EXIT_FAILURE);
            }
            for (int j = 0; j < HSDetection::Detection::max_neighbors; j++)
            {
                int curr_neighbor_channel = HSDetection::Detection::inner_neighbor_matrix[curr_max_channel][j];
                // Out of inner neighbors
                if (curr_neighbor_channel != -1)
                {
                    // Check if noise_duration is too large in comparison to the buffer size
                    int amp_cutout_size, cutout_start_index;
                    if (HSDetection::Detection::cutout_start < HSDetection::Detection::noise_duration || HSDetection::Detection::cutout_end < HSDetection::Detection::noise_duration)
                    {
                        // TODO: possible to enter this branch???
                        amp_cutout_size = HSDetection::Detection::cutout_size;
                        cutout_start_index = HSDetection::Detection::cutout_start;
                    }
                    else
                    {
                        amp_cutout_size = HSDetection::Detection::noise_duration * 2;
                        cutout_start_index = HSDetection::Detection::noise_duration;
                    }
                    int sum = 0;
                    for (int k = 0; k < amp_cutout_size; k++)
                    {
                        int curr_reading = HSDetection::Detection::trace.get(
                            curr_spike.frame - cutout_start_index + k, curr_neighbor_channel);
                        int curr_amp = (curr_reading - curr_spike.aGlobal) * HSDetection::Detection::ASCALE -
                                       curr_spike.baselines[curr_neighbor_channel];
                        if (curr_amp >= 0)
                        {
                            sum += curr_amp;
                        }
                    }
                    com_cutouts[i].push_back(make_pair(curr_neighbor_channel, sum));
                }
                // Out of neighbors to add cutout for
                else
                {
                    break;
                }
            }
        }
        curr_spike.waveforms = com_cutouts;
        return curr_spike;
    }

}
