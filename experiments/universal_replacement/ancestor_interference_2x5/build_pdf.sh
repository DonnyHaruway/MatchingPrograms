#!/bin/sh
set -eu
task_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
task_tmp=$(mktemp -d /private/tmp/ancestor-interference-pdf.XXXXXX)
mkdir -p "$task_tmp/texcache" "$task_dir/output/pdf"
TEXMFCACHE="$task_tmp/texcache" TEXMFVAR="$task_tmp/texcache" \
    lualatex -interaction=nonstopmode -halt-on-error -output-directory="$task_tmp" "$task_dir/scenario.tex"
cp "$task_tmp/scenario.pdf" "$task_dir/output/pdf/two_level_ancestor_interference.pdf"
printf 'PDF: %s\nBuild log and intermediates: %s\n' "$task_dir/output/pdf/two_level_ancestor_interference.pdf" "$task_tmp"
