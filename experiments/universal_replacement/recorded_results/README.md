# 保存済みの固定問題の調査結果

2026年10月7日に実行した5企業・8労働者の固定問題について、結論を裏付ける結果を保存する。
通常の実行はGit対象外の `../outputs/` に新しいディレクトリを作る。
ここには今回の2実行を選んで保存している。

- `run_1791354911937/`：全マッチング探索。個人合理的・feasibleな23,521候補のうち4,791件が安定。全員マッチの安定解は21件。
- `da_paths_1791356176635/`：改良DAの全ランダム選択経路。firmは3経路すべて安定、doctor/workerは10経路中8経路が安定。

各ディレクトリに元の問題、集計、CSVを保存した。
DAの結果には全選択履歴と独立再計算による全ラウンドのJSONも含む。
Markdownの[全体分岐図と各経路への一覧](../modified_docs/da_paths_index.md)は`modified_docs` 内にある。

保存結果の検算（`programs` から）：

```sh
python3 experiments/universal_replacement/verify_da_paths.py experiments/universal_replacement/recorded_results/da_paths_1791356176635
```
