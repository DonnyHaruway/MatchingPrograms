# MatchingSystem.cpp — 関数ドキュメント

**変数定義（共通）**

| 記号 | 意味 |
|------|------|
| n    | エージェント数 (`n_agents`) |
| f    | 企業数 (`n_firms`) |
| C    | 最大キャパシティ |

---

## `MatchingSystem::MatchingSystem` （コンストラクタ）

```cpp
MatchingSystem(int n_agents, int n_firms)
```

### 処理内容
`n_agents` と `n_firms` を初期化するだけ。

### 計算量
**O(1)**

---

## `generate_random_*` 系の共通仕様

必須引数は選好の生成方式（`PrefKind`）のみで、残りは `std::optional` による省略可能引数。

| 引数 | 省略時の挙動 |
|------|--------------|
| `seed` | `std::random_device` から取得（再現性が必要なら明示すること） |
| `min` / `max` | `DEFAULT_SCORE_MIN = 1` / `DEFAULT_SCORE_MAX = 10`(仮) |
| `firm_kind` | `agent_kind` と同じ |
| `col_kind` | `agent_kind` と同じ（同僚選好なしにしたい場合は `PrefKind::none` を明示） |

`min` / `max` が使われるのは生成方式によって異なる。

| `PrefKind` | min/max の扱い | 対応する向き |
|-----------|----------------|--------------|
| `ranked` | `min` のみ使用（`max` は無視） | opp / col |
| `numeric` | 両方使用 | opp / col |
| `binary` | 無視（拒否スコアは `-INF` 固定） | col のみ |
| `super_increasing` | 無視 | opp / col |
| `none` | 無視（すべて 0） | col のみ |

向きに対応しない `PrefKind` を渡すと `std::invalid_argument` を投げる。

---

## `MatchingSystem::generate_random_prefs`

```cpp
void generate_random_prefs(
    PrefKind agent_kind,
    std::optional<unsigned int> seed = std::nullopt,
    std::optional<PrefKind> firm_kind = std::nullopt,
    std::optional<PrefKind> col_kind = std::nullopt,
    std::optional<int> agent_score_min = std::nullopt, std::optional<int> agent_score_max = std::nullopt,
    std::optional<int> firm_score_min = std::nullopt,  std::optional<int> firm_score_max = std::nullopt,
    std::optional<int> agent_col_score_min = std::nullopt, std::optional<int> agent_col_score_max = std::nullopt)
```

### 処理内容
エージェント選好・企業選好・同僚選好を一括生成する。
内部で以下の 3 関数を順に呼び出す（3種類が同じ乱数列にならないようシードをずらす）。

1. `generate_random_agent_prefs(agent_kind, seed)`
2. `generate_random_firm_prefs(firm_kind, seed+1)`
3. `generate_random_agent_col_prefs(col_kind, seed+2)`

### 計算量
| ステップ | 計算量 |
|----------|--------|
| エージェント選好生成 | O(n · f log f)（ranked時） |
| 企業選好生成 | O(f · n log n)（ranked時） |
| 同僚選好生成 | O(n² log n)（ranked時） |
| **合計** | **O(n² log n)** |

---

## `MatchingSystem::generate_random_agent_prefs`

```cpp
void generate_random_agent_prefs(
    PrefKind kind,
    std::optional<unsigned int> seed = std::nullopt,
    std::optional<int> agent_score_min = std::nullopt,
    std::optional<int> agent_score_max = std::nullopt)
```

### 処理内容
n 人のエージェントそれぞれについて f 企業への選好ベクトルを生成する（`Side::opp`）。

- `PrefKind::ranked`: `generate_random_ranked(Side::opp, n_firms, ...)` — 順位付きランキング
- `PrefKind::numeric`: `generate_random_numeric(Side::opp, n_firms, ...)` — 数値スコア
- `PrefKind::super_increasing`: `generate_random_super_increasing(Side::opp, n_firms, ...)` — 超増加列
- `PrefKind::binary` / `PrefKind::none`: 例外

### 計算量
| kind | 計算量 |
|------|--------|
| `ranked` | O(n · f log f)（shuffle） |
| `numeric` | O(n · f) |
| `super_increasing` | O(n · f) |

---

## `MatchingSystem::generate_random_firm_prefs`

```cpp
void generate_random_firm_prefs(
    PrefKind kind,
    std::optional<unsigned int> seed = std::nullopt,
    std::optional<int> firm_score_min = std::nullopt,
    std::optional<int> firm_score_max = std::nullopt)
```

### 処理内容
f 社の企業それぞれについて n 人のエージェントへの選好ベクトルを生成する（`Side::opp`）。

### 計算量
| kind | 計算量 |
|------|--------|
| `ranked` | O(f · n log n) |
| `numeric` | O(f · n) |
| `super_increasing` | O(f · n) |

---

## `MatchingSystem::generate_random_agent_col_prefs`

```cpp
void generate_random_agent_col_prefs(
    PrefKind kind,
    std::optional<unsigned int> seed = std::nullopt,
    std::optional<int> agent_col_score_min = std::nullopt,
    std::optional<int> agent_col_score_max = std::nullopt)
```

### 処理内容
n 人のエージェントそれぞれについて他の n 人への同僚選好ベクトルを生成する（`Side::col`）。
自分自身へのスコアは常に 0 になる。
`PrefKind::none` の場合は全て 0 のベクトルを使用（同僚効果なし）。

### 計算量
| kind | 計算量 |
|------|--------|
| `ranked` | O(n² log n) |
| `numeric` | O(n²) |
| `binary` | O(n²) |
| `super_increasing` | O(n² log n) |
| `none` | O(n²)（ゼロ初期化） |

---

## `MatchingSystem::set_prefs`

```cpp
void set_prefs(
    const std::vector<std::vector<int>>& agent_prefs,
    const std::vector<std::vector<int>>& firm_prefs,
    const std::vector<std::vector<int>>& agent_col_prefs)
```

### 処理内容
3 種の選好行列を検証してからコピーセットする。
サイズ不一致時は `std::invalid_argument` を投げる。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 全行の長さ検証 | O(n · f + f · n + n²) |
| ベクトルのコピー | O(n · f + f · n + n²) |
| **合計** | **O(n² + n · f)** |

---

## `MatchingSystem::generate_random_capacities`

```cpp
void generate_random_capacities(std::optional<unsigned int> seed = std::nullopt)
```

### 処理内容
各企業のキャパシティを `[1, n_agents]` の一様分布からランダムに生成する。

### 計算量
**O(f)**

---

## `MatchingSystem::set_agent_prefs` / `set_firm_prefs` / `set_agent_col_prefs`

```cpp
void set_agent_prefs(const std::vector<std::vector<int>>& prefs)
void set_firm_prefs(const std::vector<std::vector<int>>& prefs)
void set_agent_col_prefs(const std::vector<std::vector<int>>& prefs)
```

### 処理内容
各選好行列のサイズを検証してコピーセットする。

### 計算量
| 関数 | 計算量 |
|------|--------|
| `set_agent_prefs` | O(n · f) |
| `set_firm_prefs` | O(f · n) |
| `set_agent_col_prefs` | O(n²) |

---

## `MatchingSystem::set_firm_capacities`

```cpp
void set_firm_capacities(const std::vector<int>& firm_capacities)
```

### 処理内容
企業キャパシティベクトルのサイズを検証してコピーセットする。

### 計算量
**O(f)**

---

## `MatchingSystem::make_all_matchings`

```cpp
std::vector<Matching> make_all_matchings() const
```

### 処理内容
全ての有効なマッチングを列挙してそれぞれのスコアを計算して返す。

1. `prepare_all_candidates(agent_ids, firm_capacities)` — 候補マップ構築
2. `generate_matchings_recursive(...)` — バックトラッキングで全マッチング列挙
3. 各マッチングに対して `compute_scores` を呼び出す

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `prepare_all_candidates` | O(f · 2^n · C log C) |
| `generate_matchings_recursive` | 指数的 O(Πⱼ |candidate_map[j]|) |
| 全マッチングのスコア計算 | O(M · (n + f) · C)（M = 総マッチング数） |
| **合計** | **指数的（n, f, C に対して）** |

> **注意**: 小規模インスタンス（n ≤ 10, f ≤ 5, C ≤ 4 程度）での使用を推奨。

---

## `MatchingSystem::run_algorithm`

```cpp
Matching run_algorithm(const std::string& algorithm_name) const
```

### 処理内容
アルゴリズム名に応じて対応するマッチングアルゴリズムを実行し、スコアを計算して返す。

| アルゴリズム名 | 実装関数 |
|----------------|----------|
| `"doctor_dictator"` | `doctor_dictator_algorithm(...)` |
| `"firm_dictator"` | `firm_dictator_algorithm(...)` |
| `"doctor_propose_DA"` | `doctor_proposing_DA_algorithm(...)` |

各アルゴリズムの計算量は [algorithms.md](algorithms.md) を参照（今後作成予定）。

### 計算量
各アルゴリズム依存（後述の algorithms ドキュメント参照）+ `compute_scores` O((n+f)·C)。

---

## `MatchingSystem::print_prefs`

```cpp
void print_prefs() const
```

### 処理内容
エージェント選好・企業選好・同僚選好・企業キャパシティを標準出力に表示する。

### 計算量
**O(n · f + f · n + n²)** = **O(n²)** — 全選好行列を一度走査。
