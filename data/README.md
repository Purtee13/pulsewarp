# Data

PulseWarp uses the Kaggle **ECG Heartbeat Categorization Dataset**
(https://www.kaggle.com/datasets/shayanfazeli/heartbeat), derived from MIT-BIH.

Put `mitbih_train.csv` (and optionally `mitbih_test.csv`) in this folder,
either by hand or with `scripts/get_data.sh` (needs the kaggle CLI + API token).

Format: one heartbeat per row, 187 samples at 125 Hz (normalised to [0,1],
zero-padded at the end) followed by the class label (0 N, 1 S, 2 V, 3 F, 4 Q).

How the loader uses it (`src/data.c`):
- picks N rows at a fixed stride across the file, so the subset is
  deterministic and contains every class (the file is sorted by class);
- trims the trailing zero padding (disable with `--no-trim`);
- resamples each beat to the target length L with linear interpolation.

N is held fixed across the length sweep, so length is the only variable.
The CSV files are large and are not meant to be committed.
