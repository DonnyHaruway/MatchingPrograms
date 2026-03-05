# Utils.cpp — 関数ドキュメント

**変数定義（共通）**

| 記号 | 意味 |
|------|------|
| n    | エージェント数 (`n_agents`) |
| f    | 企業数 (`n_firms`) |
| C    | 企業の最大キャパシティ |
| k    | 部分集合のサイズ |
| C(n,k) | 二項係数（n choose k）|

---

## `generate_random_ranked`

```cpp
std::vector<int> generate_random_ranked(int size, int min, std::mt19937& rng, std::string type, int who)
```

### 処理内容
- `type == "opponent"`: `[min, min+size-1]` の整数を並べてシャッフル。完全なランキング列を生成。
- `type == "col"`: 自分自身（`who`）のスコアを 0 に固定し、残りを `[1, size-1]` の順位でシャッフル。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `std::iota` | O(size) |
| `std::shuffle` | O(size) |
| **合計** | **O(size)** |

---

## `generate_random_number`

```cpp
std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937& rng, std::string type, int who)
```

### 処理内容
`[min_val, max_val]` の一様分布から乱数を `size` 個生成する。
`type == "col"` の場合は `v[who] = 0` に固定してから残りを生成。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 乱数生成ループ | O(size) |
| **合計** | **O(size)** |

---

## `generate_random_super_increasing`

```cpp
std::vector<int> generate_random_super_increasing(int size, std::mt19937& rng)
```

### 処理内容
`1, 2, 4, 8, ...`（2のべき乗）の超増加列を生成し、シャッフルして返す。
`size > 30` の場合はオーバーフロー防止のため例外を投げる。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 列生成ループ | O(size) |
| `std::shuffle` | O(size) |
| **合計** | **O(size)** |

---

## `generate_random_binary`

```cpp
std::vector<int> generate_random_binary(int size, std::mt19937& rng, std::string type, int who)
```

### 処理内容
各要素を確率 1/2 で `0` または `-1e9` に設定する。
`type == "col"` の場合は `v[who] = 0` に固定。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 乱数生成ループ | O(size) |
| **合計** | **O(size)** |

---

## `generate_combinations` ★最適化済み

```cpp
std::vector<std::set<int>> generate_combinations(const std::vector<int>& set, int k)
```

### 処理内容
整数ベクトル `set` からサイズ `k` の全組み合わせを列挙して返す。

### 最適化
| | 実装 | 計算量 |
|---|---|---|
| **変更前** | ビットマスク + `std::next_permutation` | O(C(n,k) · n) |
| **変更後** | 再帰的バックトラック (`combinations_helper`) | **O(C(n,k) · k)** |

再帰実装では各組み合わせを直接構築するため、`next_permutation` の O(n) コスト（全 bitmask スキャン）が不要になり、**n / k 倍の高速化**が期待できる。

### 変更後の計算量
| ステップ | 計算量 |
|----------|--------|
| 全組み合わせ列挙 | O(C(n,k)) |
| 各組み合わせの `set<int>` 構築 | O(k log k) |
| **合計** | **O(C(n,k) · k log k)** |

---

## `generate_all_subsets_by_size`

```cpp
std::map<int, std::vector<std::set<int>>> generate_all_subsets_by_size(
    const std::vector<int>& set, int max_size)
```

### 処理内容
サイズ `0` から `max_size` までの全部分集合を列挙し、サイズをキーとする `map` で返す。
内部で `generate_combinations(set, k)` を `k = 0..max_size` の回数呼び出す。

### 計算量
サイズ `k` の部分集合数の総和：

$$\sum_{k=0}^{C} \binom{n}{k} \leq 2^n$$

| ステップ | 計算量 |
|----------|--------|
| 各サイズの `generate_combinations` 呼び出し | O(Σ C(n,k) · k log k) |
| **合計（上界）** | **O(2^n · C log C)** |

> **注意**: max_size = n の場合は全部分集合 = O(2^n) となる指数的コスト。実用上は max_size = C（企業キャパシティ）で C ≪ n を想定。

---

## `prepare_all_candidates`

```cpp
std::vector<std::vector<std::set<int>>> prepare_all_candidates(
    const std::vector<int>& agent_ids,
    const std::vector<int>& firm_capacities)
```

### 処理内容
全エージェントの部分集合を最大キャパシティまで列挙し、各企業のキャパシティに応じた候補リストを構築する。
1. `generate_all_subsets_by_size(agent_ids, max_capacity)` を1回呼び出す
2. 各企業について、容量 0..cap の部分集合を結合して返す

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `generate_all_subsets_by_size` | O(2^n · C log C) |
| 企業ごとのマージ（最悪 f 回） | O(f · 2^n) |
| **合計** | **O(f · 2^n · C log C)** |

---

## `compute_firm_score`

```cpp
int compute_firm_score(const std::set<int>& firm_match, const std::vector<int>& firm_pref)
```

### 処理内容
企業とマッチしたエージェント集合のスコア合計を返す。各エージェント `a` に対して `firm_pref[a]` を加算する。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 集合の走査 | O(\|firm_match\|) |
| **合計** | **O(k)** (k = マッチ数) |

---

## `compute_agent_score`

```cpp
int compute_agent_score(
    const int& firm,
    const std::set<int>& agent_col_match,
    const std::vector<int>& agent_pref,
    const std::vector<int>& agent_col_pref)
```

### 処理内容
エージェントの総スコアを計算する。
- マッチした企業からのスコア: `agent_pref[firm]`
- 同僚エージェントからのスコア: 各同僚 `c` に対して `agent_col_pref[c]` を加算

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 同僚集合の走査 | O(\|agent_col_match\|) |
| **合計** | **O(k)** (k = 同僚数) |
