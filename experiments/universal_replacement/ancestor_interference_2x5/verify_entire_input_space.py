"""Independently check graph-cover certificates for ALL preference/graph inputs.

For each preference profile, a finite collection of legal deletion paths has
original-UR-stable leaves. The verifier computes the entire family of forbidden
graphs on which each path is legal and stable, then checks that their union
contains all 1024 graphs. It generates no branches using the C++ tree solver.
"""
import argparse
import gzip
import itertools
import json
import lzma
import struct
import time
from pathlib import Path

from verify_all_graph_certificates import ALL, BatchDA, TYPES, W


def verify(mode, certificate, compare):
    started = time.monotonic()
    base = 2 if mode == 'complete' else 5
    total = base ** W
    opener = lzma.open if certificate.suffix == '.xz' else gzip.open if certificate.suffix == '.gz' else open
    profiles = covered_inputs = path_count = 0
    header, path_record = struct.Struct('<IHH'), struct.Struct('<B5s')
    with opener(certificate, 'rb') as source:
        assert source.read(4) == b'URG1', 'global certificate header'
        for perm_index, perm in enumerate(itertools.permutations(range(W))):
            firm = (tuple(range(W, 0, -1)), tuple(W - perm.index(w) for w in range(W)))
            for code in range(total):
                expected_profile = perm_index * total + code
                profile, count, failures = header.unpack(source.read(header.size))
                assert profile == expected_profile, 'missing or misordered preference profile'
                assert failures == 0, 'this checker verifies the zero-failure conclusion'
                n, worker = code, []
                for _ in range(W):
                    worker.append(TYPES[n % base])
                    n //= base
                model = BatchDA(worker, firm)
                covered = 0
                for _ in range(count):
                    length, packed = path_record.unpack(source.read(path_record.size))
                    covered |= model.path_family(length, int.from_bytes(packed, 'little'))
                assert covered == ALL, ('some forbidden graphs lack a stable-branch certificate', profile)
                profiles += 1
                covered_inputs += 1024
                path_count += count
            if (perm_index + 1) % 10 == 0:
                print(f'permutations={perm_index+1} profiles={profiles} '
                      f'verified_inputs={covered_inputs} elapsed={time.monotonic()-started:.1f}s', flush=True)
        assert not source.read(1), 'trailing global certificate data'
    cpp = json.loads(compare.read_text())
    assert profiles == cpp['preference_profiles']
    assert covered_inputs == cpp['all_profile_graph_inputs']
    assert path_count == cpp['stable_path_certificates']
    assert cpp['all_branches_fail_inputs'] == 0
    return dict(mode=mode, preference_profiles_verified=profiles,
                all_profile_graph_inputs_verified=covered_inputs,
                stable_deletion_paths_verified=path_count,
                all_1024_graphs_covered_for_every_preference_profile=True,
                every_input_has_an_independently_verified_original_ur_stable_branch=True,
                all_branches_fail_inputs=0, no_two_step_interference_filter_used=True,
                cpp_counts_match=True, elapsed_seconds=time.monotonic()-started)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', choices=['complete', 'incomplete'], required=True)
    parser.add_argument('--certificate', type=Path, required=True)
    parser.add_argument('--compare', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.mode, args.certificate, args.compare)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
