from pathlib import Path
from typing import Mapping, Union

import numpy as np
import spikeinterface.toolkit as st
from myherdingspikes import HS2Detection
from myherdingspikes.recording import Recording
from numpy.typing import NDArray

default_kwargs = {
    # core params
    'left_cutout_time': 0.3,
    'right_cutout_time': 1.8,
    'detect_threshold': 20,

    # extra probe params
    'probe_masked_channels': None,
    'probe_inner_radius': 70,
    'probe_neighbor_radius': 90,
    'probe_event_length': 0.26,
    'probe_peak_jitter': 0.2,
    'noise_amp_percent': 1.0,

    # extra detection params
    't_inc': 100000,
    'num_com_centers': 1,
    'maa': 12,
    'ahpthr': 11,
    'out_file_name': 'HS2_detected',
    'decay_filtering': False,
    'save_all': False,
    'amp_evaluation_time': 0.4,
    'spk_evaluation_time': 1.0,
    'to_localize': True,

    # bandpass filter
    'freq_min': 300.0,
    'freq_max': 6000.0,
    'filter': True,

    # rescale traces
    'pre_scale': True,
    'pre_scale_value': 20.0,
}


def run_hs2(recording: Recording, output_folder: Union[str, Path] = 'result_HS2', **kwargs
            ) -> Mapping[str, Union[NDArray[np.integer], NDArray[np.floating]]]:
    params = default_kwargs.copy()
    params.update(kwargs)
    params['out_file_name'] = Path(output_folder) / params['out_file_name']

    if params['filter'] and params['freq_min'] is not None and params['freq_max'] is not None:
        recording = st.bandpass_filter(
            recording, freq_min=params['freq_min'], freq_max=params['freq_max'])

    H = HS2Detection(
        recording,
        masked_channels=params['probe_masked_channels'],
        noise_amp_percent=params['noise_amp_percent'],
        inner_radius=params['probe_inner_radius'],
        neighbor_radius=params['probe_neighbor_radius'],
        event_length=params['probe_event_length'],
        peak_jitter=params['probe_peak_jitter'],
        left_cutout_time=params['left_cutout_time'],
        right_cutout_time=params['right_cutout_time'],
        threshold=params['detect_threshold'],
        to_localize=params['to_localize'],
        num_com_centers=params['num_com_centers'],
        maa=params['maa'],
        ahpthr=params['ahpthr'],
        out_file=params['out_file_name'],
        decay_filtering=params['decay_filtering'],
        save_all=params['save_all'],
        amp_evaluation_time=params['amp_evaluation_time'],
        spk_evaluation_time=params['spk_evaluation_time'],
        t_inc=params['t_inc'],
        pre_scale=params['pre_scale'],
        pre_scale_value=params['pre_scale_value']
    )

    return H.detect()
