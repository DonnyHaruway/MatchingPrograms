# MatchingUtils.cpp — 関数ドキュメント

**変数定義（共通）**

| 記号 | 意味 |
|------|------|
| n    | エージェント数 (`n_agents`) |
| f    | 企業数 (`n_firms`) |
| C_j  | 企業 j のキャパシティ |
| C    | 最大キャパシティ = max(C_j) |
| M    | 生成される全マッチング数（指数的） |

---

## `generate_matchings_recursive`

```cpp
void generate_matchings_recursive(
    int firm_idx,
    const int& n_agents,
    const std::vector<std::vector<std::set<int>>>& candidate_map,
    std::vector<std::set<int>>& current_matching,
    std::set<int>& used_agents,
    std::vector<Matching>& result
)
```

### 処理内容
バックトラッキングにより、全ての有効なマッチングを再帰的に列挙する。

1. `firm_idx == candidate_map.size()` になったら現在のマッチングを `result` に追加。
2. 各企業について `candidate_map[firm_idx]` の候補グループを順に試す。
3. **エージェント重複チェック**: 候補グループ内の全エージェントが `used_agents` に未登録なら有効。
4. 有効なら `used_agents` に追加して次の企業を再帰処理 → バックトラック。

### 計算量

#### 候補マップのサイズ
企業 j の候補グループ数は $\sum_{k=0}^{C_j} \binom{n}{k}$。全企業で最大 $\left(\sum_{k=0}^{C} \binom{n}{k}\right)^f$ 組み合わせが存在する。

| ステップ | 計算量 |
|----------|--------|
| 各ノードでの重複チェック | O(C) — 候補グループのエージェント数 |
| 再帰ツリーの全ノード数 | O(M · f) — M は実際の有効マッチング数 |
| `Matching::from_firm_assignment`（葉ノード）| O(n + C_total) |
| **合計（上界）** | **指数的 O(Πⱼ \|candidate_map[j]\| · (C + n))** |

#### 枝刈りの効果
- `used_agents`（`std::set<int>`）により O(log n) でエージェント重複を検出。
- 重複があれば即座に枝刈りするため、実際のマッチング数は理論上界より大幅に少ない。

### 備考
- この関数は `MatchingSystem::make_all_matchings` から呼び出される。
- 小規模なシミュレーション（n ≤ 10, f ≤ 5 程度）を想定した設計。
- `candidate_map` は `prepare_all_candidates` で事前構築される。
