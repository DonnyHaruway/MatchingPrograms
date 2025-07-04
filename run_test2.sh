#!/bin/bash

output_file="../test_results2.txt"
> "$output_file"  # 既存の内容をクリア（空にする）

echo "=== Run #1 ===" >> "$output_file"
./tests/test_algorithms_dictator >> "$output_file" 2>&1
echo "" >> "$output_file"