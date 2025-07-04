#!/bin/bash

output_file="../test_results.txt"
> "$output_file"  # 既存の内容をクリア（空にする）

for i in {1..100}
do
  echo "=== Run #$i ===" >> "$output_file"
  ./tests/test_algorithms_dictator >> "$output_file" 2>&1
  echo "" >> "$output_file"
done