#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROGRAMS_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROGRAMS_ROOT}/build/stable"
OUTPUT_DIR="${SCRIPT_DIR}/outputs"

TRIALS="${1:-100000}"
SEED="${2:-20260828}"
INFEASIBLE_PAIR_COUNT="${3:-1}"
MODE="${4:-random}"

if [[ "${MODE}" != "random" && "${MODE}" != "exhaustive" ]]; then
  printf 'mode must be random or exhaustive: %s\n' "${MODE}" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"

RUN_TIMESTAMP="$(date '+%Y%m%d_%H%M%S')"
OUTPUT_STEM="counterexample_2x3_${MODE}_pairs${INFEASIBLE_PAIR_COUNT}_trials${TRIALS}_seed${SEED}_${RUN_TIMESTAMP}"
OUTPUT_FILE="${OUTPUT_DIR}/${OUTPUT_STEM}.txt"

# 同じ条件の実行が同じ秒に重なっても、既存ファイルを上書きしない。
SUFFIX=1
while [[ -e "${OUTPUT_FILE}" ]]; do
  OUTPUT_FILE="${OUTPUT_DIR}/${OUTPUT_STEM}_${SUFFIX}.txt"
  SUFFIX=$((SUFFIX + 1))
done

# ビルドログ、探索条件、反例の選好、全マッチングの診断を同じtxtに保存する。
exec > >(tee "${OUTPUT_FILE}") 2>&1

printf 'output file: %s\n' "${OUTPUT_FILE}"
printf 'mode: %s, trials: %s, seed: %s, infeasible pairs: %s\n' \
  "${MODE}" "${TRIALS}" "${SEED}" "${INFEASIBLE_PAIR_COUNT}"

cmake \
  -S "${SCRIPT_DIR}" \
  -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DIR}" --target search_stable_counterexample_2x3 -j

SEARCH_ARGS=(
  --trials "${TRIALS}"
  --seed "${SEED}"
  --infeasible-pairs "${INFEASIBLE_PAIR_COUNT}"
)

if [[ "${MODE}" == "exhaustive" ]]; then
  SEARCH_ARGS+=(--exhaustive)
fi

"${BUILD_DIR}/search_stable_counterexample_2x3" "${SEARCH_ARGS[@]}"
