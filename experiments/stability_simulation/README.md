# Matching stability simulation

This experiment evaluates `simple_match` (firm-proposing) and
`doctor_dictator` (worker-proposing) on the same preference profile. Every
capacity-feasible matching is also evaluated with both `Matching::is_stable`
and `Matching::is_weakly_stable`, regardless of either algorithm's result.

## Build and run

From the repository root:

```sh
cmake -S . -B build
cmake --build build --target simulate_matching_stability -j4
./build/experiments/stability_simulation/simulate_matching_stability \
  --case 3x5 --trials 100 --seed 20260902 \
  --output experiments/stability_simulation/outputs
```

Options:

- `--case 2x3|3x5|all` (default `all`)
- `--trials N`: number of random 3x5 profiles (default `18,432`); the 2x3
  case is always fully exhaustive
- `--seed N` (default `20260902`)
- `--output DIR` (default `experiments/stability_simulation/outputs`)
- `--firm-scores linear|superincreasing` (default `linear`): firm score
  scale; see "Firm score modes" below
- `--help`

Each invocation creates a new `run_YYYYMMDD_HHMMSS_seedN[_suffix]` directory,
so existing results are not overwritten. With `--case all`, both cases are
stored below the same run directory in `2x3/` and `3x5/`.

## Preference domain

Workers and firms have strict complete rankings over all alternatives on the
other side. Scores are positive, with the best alternative receiving the
number of real alternatives and the worst receiving 1. The outside option has
score 0. Thus, for example, a worker in the 3-firm case uses scores 3, 2, 1,
and a firm in the 5-worker case uses 5, 4, 3, 2, 1.

Colleague preferences are directed binary scores. Diagonal entries are 0 and
each off-diagonal entry is independently either 0 (acceptable) or `-INF`
(rejected). Consequently asymmetric rejection is included.

### Firm score modes

With `--firm-scores linear` (the default), firm scores are the rank scores
described above. With `--firm-scores superincreasing`, each firm rank score
`r` is replaced by `2^(r-1)` after the profile is generated; in the 5-worker
case the scores are 16, 8, 4, 2, 1, so each score exceeds the sum of all lower
ones. Worker scores and colleague preferences are unchanged. Because the
transform is applied after generation, a given `(seed, trial)` has the same
ordinal profile in both modes, so runs can be compared instance by instance.
Non-linear runs get a `_firm<mode>` run-directory suffix, and the mode is
recorded as `firm_scores` in `summary.txt`, as the last `summary.csv` column,
and in each JSONL record.

### Exhaustive 2x3 case

- firms/workers: 2/3
- capacities: `[2,1]`
- worker rankings: `(2!)^3 = 8`
- firm rankings: `(3!)^2 = 36`
- directed colleague matrices: `2^(3*2) = 64`
- profiles: `8 * 36 * 64 = 18,432`
- capacity-feasible matchings: 19

The generator enumerates all 18,432 profiles in a deterministic mixed-radix
order. A test verifies that all generated profiles are distinct.

### Random 3x5 case

- firms/workers: 3/5
- capacities: `[2,2,1]`
- default trials: 18,432, sampled with replacement
- capacity-feasible matchings: 456

For every trial, each worker ranking and firm ranking is an independent uniform
random permutation. Each of the 20 directed off-diagonal colleague entries is
independently rejected with probability 0.5, which is uniform over all binary
matrices.

The profile RNG is initialized from the base seed and trial number, rather than
from a shared sequential RNG. Therefore the profile for a given `(seed, trial)`
does not depend on execution order, future problem-level parallelism, or thread
count. `doctor_dictator` normally draws a random priority order internally; the
experiment uses its explicit-seed overload and records `doctor_priority_seed`
for each instance. Its randomized-priority semantics are retained while the
output becomes reproducible.

## Exhaustive matching evaluation

Assignment vectors are generated once per case in deterministic order, but a
fresh `Matching` object is constructed for every candidate and preference
profile. This is required because `Matching` caches computed scores. The
program checks at startup that the enumerator returns exactly 19 candidates for
2x3 or 456 for 3x5. It then evaluates both stability predicates for every
candidate; it does not stop after finding a witness.

`stable_witness` and `weakly_stable_witness` are the first matching found in
that deterministic enumeration order. They prove existence; they are not an
optimization result.

## Outputs

Each case directory contains:

- `summary.txt`: human-readable conditions, rates, conditional rates,
  success/miss categories, matching-count distributions, statistics by
  directed rejection count K, consistency warnings, and elapsed time
- `summary.csv`: one machine-readable aggregate row for that case/run
- `instances.jsonl`: one complete JSON object per profile
- `interesting_cases.jsonl`: complete records for missed existing solutions,
  missing solutions, or consistency warnings, with a `reasons` array

The JSONL record includes identifiers and generation metadata; dimensions and
capacities; all three preference matrices; K and `K/(|W|*(|W|-1))`; both
algorithm outputs and their two independent Boolean stability results; and the
full-search counts, existence Booleans, witnesses, and consistency warnings.
A matching is encoded as:

```json
{"firms":[[0,2],[1]],"unmatched":[]}
```

The program never collapses stability and weak stability into an exclusive
label. In particular, every `is_stable=true && is_weakly_stable=false` result
is retained, counted, and makes the instance interesting. The following five
categories are computed separately for stability and weak stability and are
checked to sum to all profiles: `both_success`, `simple_only`, `doctor_only`,
`both_missed`, and `no_solution`. A zero conditional-rate denominator is shown
as `N/A` in `summary.txt` and as `nan` in CSV.

Generated output and build artifacts are ignored by the repository's existing
`outputs/` and `build/` ignore rules.

## Known algorithm constraints

- `simple_match` intentionally retains its current queue behavior: when a
  worker moves, the old firm is not re-queued. This experiment does not change
  that semantic behavior.
- `doctor_dictator` uses a randomized worker priority. The normal unseeded API
  still uses `std::random_device`; only this experiment invokes the seeded
  overload.
- The stability predicates are the repository's existing definitions. In
  particular, weak blocking at a full firm uses its existing worst-incumbent
  replacement rule. Any unexpected relationship between the two predicates is
  reported rather than normalized away.
