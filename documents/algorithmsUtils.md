# algorithmsUtils.cpp — 関数ドキュメント

**変数定義（共通）**

| 記号 | 意味 |
|------|------|
| $n$    | エージェント数 |
| $f$    | 企業数 |
| $C$    | 最大キャパシティ |
| $C_j$  | 企業 j のキャパシティ |
| $k$    | 企業の現在のマッチ数 |
| `FirmMatching` | `std::pair<int, std::set<int>>` — (企業 id, エージェント集合) |

---

## `create_current_firm_match_subsets`

```cpp
std::vector<FirmMatching> create_current_firm_match_subsets(
    const std::vector<std::set<int>>& firm_matchs,
    const std::vector<int>& firm_capacities)
```

### 処理内容
各企業の現在のマッチングの**容量内の全部分集合**を列挙し、`FirmMatching` のリストとして返す。

- 企業 j の現マッチ集合を `vector<int>` に変換
- `generate_all_subsets_by_size(firm_match, C_j)` を呼び出し、サイズ 0 〜 C_j-1 の部分集合を収集
- サイズ = C_j の部分集合は除外（追加枠がないため）

**用途**: `doctor_dictator_algorithm` 内で、各エージェントが提案できる (企業, 部分集合) の候補リストを構築するために使用。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 各企業の `generate_all_subsets_by_size` | O(2^k · k log k) per firm |
| **合計（全企業）** | **O(f · 2^C · C log C)** |

> **注意**: 企業の現マッチ数 k は最大 C なので 2^C が上界。k が小さいほど高速。

---

## `find_prefered_match`

```cpp
FirmMatching find_prefered_match(
    const int& agent,
    const std::vector<int>& agent_pref,
    const std::vector<int>& agent_col_pref,
    const std::vector<FirmMatching>& all_firm_match,
    const std::set<FirmMatching>& unofferable)
```

### 処理内容
エージェントにとって最もスコアが高い `FirmMatching` を線形探索で見つける。

1. `all_firm_match` の全候補を走査
2. `unofferable` に含まれるもの（過去に拒否済みなど）はスキップ
3. `compute_agent_score` でスコアを計算し、最高スコアの `FirmMatching` を返す
4. スコアが 0 以下のものは採用しない（アンマッチより悪い場合）

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `all_firm_match` の走査 | O(\|all_firm_match\|) |
| `unofferable` の検索（`std::set::count`） | O(log\|unofferable\|) per entry |
| `compute_agent_score` | O(C) per entry |
| **合計** | **O(\|all_firm_match\| · (log\|unofferable\| + C))** |

> `|all_firm_match|` は `create_current_firm_match_subsets` の出力サイズ = O(f · 2^C)。

---

## `firm_match_accept_propose`

```cpp
bool firm_match_accept_propose(
    const int& agent,
    const FirmMatching& prefered_match,
    const std::set<int>& firm_match_before,
    const std::vector<std::vector<int>>& agent_prefs,
    const std::vector<std::vector<int>>& firm_prefs,
    const std::vector<std::vector<int>>& agent_col_prefs)
```

### 処理内容
エージェント `agent` が企業 `prefered_match.first` の部分集合 `prefered_match.second` に参加する提案を、
その企業と既存のメンバー全員が受け入れるかどうかを判定する。

**条件**:
1. 企業スコア: `firm_score(match_after) >= firm_score(match_before)`
2. 既存メンバー全員のスコア: `agent_score(match_after) >= agent_score(match_before)` （各人 ≥ 0 ではなく現状維持以上）

**注意**: `match_after = prefered_match.second ∪ {agent}`

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `compute_firm_score`（before/after） | O(k) each |
| 既存メンバー全員のスコア比較 | O(k) 回 |
| 各 `compute_agent_score` | O(k) |
| **合計** | **O(k²)** （k = `prefered_match.second` のサイズ ≤ C） |

→ 上界: **O(C²)**

---

## `find_agent_deleted`

```cpp
std::set<int> find_agent_deleted(
    const std::set<int>& firm_match_before,
    const std::set<int>& firm_match_after)
```

### 処理内容
`firm_match_before` にあって `firm_match_after` にいないエージェントの集合を返す（削除されたエージェントの集合）。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `firm_match_before` の走査 | O(k) |
| `firm_match_after.count` 検索 | O(log k) each |
| **合計** | **O(k · log k)** （k = before のサイズ ≤ C） |

→ 上界: **O(C · log C)**

---

## `agents_accept_match`

```cpp
bool agents_accept_match(
    const std::set<int>& candidate_set,
    const int& firm,
    const std::vector<std::vector<int>>& agent_prefs,
    const std::vector<std::vector<int>>& firm_prefs,
    const std::vector<std::vector<int>>& agent_col_prefs)
```

### 処理内容
`candidate_set` 内の全エージェントが企業 `firm` とのマッチングを受け入れるか（スコアが 0 以上か）を確認する。

- 各エージェントについて `compute_agent_score(firm, candidate_set, ...)` を計算
- スコア < 0 のエージェントが一人でもいれば `false` を返す

**用途**: `firm_dictator_algorithm` で候補集合の受け入れ可否を判定するために使用。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `candidate_set` の走査 | O(k) 回 |
| 各 `compute_agent_score` | O(k) |
| **合計** | **O(k²)** （k = `candidate_set` のサイズ ≤ C） |

→ 上界: **O(C²)**
