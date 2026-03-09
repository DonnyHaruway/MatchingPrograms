# Matching.cpp — 関数ドキュメント

**変数定義（共通）**

| 記号 | 意味 |
|------|------|
| $n$    | エージェント数 (`n_agent`) |
| $f$    | 企業数 (`n_firm`) |
| $C$    | 最大キャパシティ |
| $C_{total}$ | 全企業のキャパシティ総和 |
| k    | 企業とのマッチ数（または同僚数） |

---

## `Matching::Matching` （コンストラクタ）

```cpp
Matching(const std::vector<int>& agent_matchs, const std::vector<std::set<int>>& firm_matchs)
```

### 処理内容
エージェントのマッチ先企業と企業のマッチ済みエージェント集合を受け取り、
各エージェントの同僚集合 `agent_col_matchs` を構築する。

- 各エージェント `i` について、マッチ企業の `firm_matchs[firm]` を走査し、自分以外を同僚として登録。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 外側ループ（エージェント数） | O(n) |
| 内側ループ（企業のマッチ数） | O(C_j) per agent |
| `set::insert` | O(log C) per insertion |
| **合計** | **O(n · C · log C)** |

---

## `Matching::from_firm_assignment` （静的ファクトリ）

```cpp
static Matching from_firm_assignment(
    const std::vector<std::set<int>>& input_firm_match, const int& n_agents)
```

### 処理内容
企業→エージェント集合の割り当てから `Matching` オブジェクトを生成する。
各企業の `set<int>` を走査してエージェントのマッチ先企業 `agent_matchs` を構築し、コンストラクタを呼ぶ。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 全企業のエージェント走査 | O(C_total) |
| コンストラクタ呼び出し | O(n · C · log C) |
| **合計** | **O(n · C · log C)** |

---

## `Matching::compute_scores`

```cpp
void compute_scores(
    const std::vector<std::vector<int>>& agent_prefs,
    const std::vector<std::vector<int>>& firm_prefs,
    const std::vector<std::vector<int>>& agent_col_prefs)
```

### 処理内容
全エージェントと全企業のスコアを一括計算してキャッシュする。
2 回目以降の呼び出しは `scores_computed` フラグにより即時リターン（O(1)）。

- エージェントスコア: `compute_agent_score(matched_firm, colleagues, pref, col_pref)`
- 企業スコア: `compute_firm_score(matched_agents, pref)`

### 計算量
| ステップ | 計算量 |
|----------|--------|
| 全エージェントのスコア計算 | O(n · C) |
| 全企業のスコア計算 | O(f · C) |
| **合計（初回）** | **O((n + f) · C)** |
| **2回目以降** | **O(1)** |

---

## `Matching::is_individually_rational`

```cpp
bool is_individually_rational(
    const std::vector<std::vector<int>>& agent_prefs,
    const std::vector<std::vector<int>>& firm_prefs,
    const std::vector<std::vector<int>>& agent_col_prefs)
```

### 処理内容
全エージェントと全企業のスコアが 0 以上（＝アンマッチより悪くない）かを確認する。
内部で `compute_scores` を呼び出す。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `compute_scores` | O((n + f) · C)（初回のみ） |
| スコアの線形チェック | O(n + f) |
| **合計** | **O((n + f) · C)** |

---

## `Matching::blocking_pairs` ★最適化済み

```cpp
std::vector<std::pair<int,int>> blocking_pairs(
    const std::vector<std::vector<int>>& agent_prefs,
    const std::vector<std::vector<int>>& firm_prefs,
    const std::vector<std::vector<int>>& agent_col_prefs,
    const std::vector<int>& firm_capacities)
```

### 処理内容
全ての (agent, firm) ペアについてブロッキングペアかどうかを判定する。

ブロッキングペアの条件（`(agent, firm)` が blocking pair）:
1. 企業の現マッチの部分集合 `S` が存在して `S ∪ {agent}` が容量以内
2. 企業にとって `firm_score(S ∪ {agent}) > firm_score(current_match)`
3. `agent` 自身にとっても `agent_score(firm, S ∪ {agent}) > current_score[agent]`
4. `S` 内の全員がこの変更を受け入れる（スコアが下がらない）

### 最適化

| | 実装 | `generate_all_subsets_by_size` の呼び出し回数 |
|---|---|---|
| **変更前** | firm ループ × agent ループ内で生成 | **n × f 回** |
| **変更後** | firm ループ内で1回だけ生成（agent ループ外） | **f 回** |

→ サブセット生成コストを **n 分の 1** に削減。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `compute_scores` | O((n + f) · C)（初回のみ） |
| firm ごとのサブセット生成（f 回） | O(f · 2^C · C) |
| (agent, firm, subset) の全組み合わせ評価 | O(n · f · 2^C · C) |
| 各サブセット評価（`compute_firm_score` + `compute_agent_score`） | O(C) each |
| **合計** | **O(n · f · 2^C · C)** |

> **備考**: `2^C` の指数的コストはキャパシティ C に依存する。C が小さい（C ≤ 10 程度）場合は実用的。

---

## `Matching::is_stable`

```cpp
bool is_stable(
    const std::vector<std::vector<int>>& agent_prefs,
    const std::vector<std::vector<int>>& firm_prefs,
    const std::vector<std::vector<int>>& agent_col_prefs,
    const std::vector<int>& firm_capacities)
```

### 処理内容
1. `is_individually_rational` で全員がマッチを受け入れているか確認
2. `blocking_pairs` でブロッキングペアがないか確認

両方通れば安定マッチング。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `is_individually_rational` | O((n + f) · C) |
| `blocking_pairs` | O(n · f · 2^C · C) |
| **合計** | **O(n · f · 2^C · C)** |

---

## `Matching::print`

```cpp
void print() const
```

### 処理内容
マッチング情報（エージェントのマッチ先、企業のマッチ済みエージェント、同僚、スコア）を標準出力に表示する。

### 計算量
**O(n + f + C_total)** — 全エージェント・企業の情報を一度走査。

---

## `Matching::operator==`

```cpp
bool operator==(const Matching& other) const
```

### 処理内容
`agent_matchs` と `firm_matchs` が両方とも一致するか比較する。

### 計算量
| ステップ | 計算量 |
|----------|--------|
| `vector<int>` の比較 | O(n) |
| `vector<set<int>>` の比較 | O(f + C_total) |
| **合計** | **O(n + f + C_total)** |

---

## アクセッサ群

`get_agent_matchs`, `get_firm_matchs`, `get_agent_col_matchs`, `get_agent_scores`, `get_firm_scores`

### 処理内容
メンバ変数への const 参照を返す。スコア系はスコア未計算時に例外を投げる。

### 計算量
いずれも **O(1)**（参照返し）。
