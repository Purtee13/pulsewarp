#!/usr/bin/env bash
# Fetch the Kaggle "ECG Heartbeat Categorization Dataset" (shayanfazeli/heartbeat)
# into data/. Needs the kaggle CLI and ~/.kaggle/kaggle.json (API token).
# Without it: download the zip from
#   https://www.kaggle.com/datasets/shayanfazeli/heartbeat
# and copy mitbih_train.csv (and mitbih_test.csv) into data/.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p data
kaggle datasets download -d shayanfazeli/heartbeat -p data --unzip
ls -lh data/mitbih_*.csv
