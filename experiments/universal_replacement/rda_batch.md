# RDAの5×8複数ケース実験

[本実験の結果（各1,000件）](recorded_results/rda_batch_20261021_seed20261021/main/report.md) / [企業告白型の反例](recorded_results/rda_batch_20261021_seed20261021/main/counterexamples.md) / [予備実験（各100件）](recorded_results/rda_batch_20261021_seed20261021/pilot/report.md)

## 実験条件

- 企業5・労働者8、定員は固定サンプルと同じ `(2,2,2,2,1)`。
- 全28組の労働者ペアのうち9組を一様ランダムに選び、相互拒否にする。他の同僚選好は0。
- 完全選好と不完全選好の各1,000問題に、企業告白型・個人告白型RDAをそれぞれ適用する。
- RDAは違反ペアがなくなるまで全違反ペアの両削除対象を再帰的に探索する。各ノードの枝数は違反ペア数×2。重複する枝も探索する。
- 安定解発見による打ち切り、枝刈り、状態の統合は行わない。元の選好に対してUR判定し、空席への追加逸脱も含める。
- 図は生成しない。入力と証拠経路を保存し、必要な問題を後から再現できるようにする。

同僚選好の9組は市場全体の禁止ペア数。DA結果で同じ企業に所属した実際の違反ペア数は固定せず、提案側ごとに `initial_conflicts` として記録する。

## 選好の分布と再現性

各労働者について企業1〜5、各企業について労働者1〜8を独立に一様シャッフルする。
完全選好ではこの順列全体を使い、すべての相手を受け入れ可能とする。
不完全選好では同じ順列を、固定サンプルと同じ長さに切り詰める。

| 所有者 | 不完全選好での受け入れ可能人数 |
|---|---|
| w1〜w8 | 4, 3, 4, 4, 2, 4, 4, 2 |
| f1〜f5 | 7, 6, 7, 3, 4 |

順列の先頭k人は、長さkの順序付きリストの一様抽出に相当する。
各リストの順位に対して、最上位をリスト長、最下位を1とする正のスコアを付ける。
リスト外は `-INF`、未マッチの評価は0。同順位はない。
企業と労働者の受け入れ可能性はそれぞれ生成し、片方だけが受け入れ不可の関係も含める。

基準seedは `20261021`。試行番号は0始まり。
同じ `(seed, trial)` の完全・不完全選好は、順位の基礎順列と禁止ペアを共有する。
試行ごとに独立した乱数生成器を作るため、試行数を変更しても先頭部分の問題は変わらない。
順位用と同僚選好用の乱数列も分けている。
各条件内では復元抽出とし、重複する入力を人為的に除外しない。

seedによる再生成は同じ実行環境で再現できる。標準ライブラリのシャッフル実装の違いにも対応できるよう、全選好行列と順位表も保存する。

## 実施した順序

1. 各条件100件（計200問題・400探索）の予備実験で探索量と結果を確認。
2. 各条件1,000件（計2,000問題・4,000探索）の本実験を実施。
3. 全問題の入力・経路数・安定終点数・異なる終点数・最大深さ等を独立Python実装で検算。
4. 各問題・提案側について、存在すれば安定/不安定な証拠経路を各1本保存し、全DA・削除・元の選好での逸脱を再計算。
5. 予備実験と本実験の先頭100件ずつについて、時間以外の入力・結果の一致を確認。

本実験の件数には予備実験を重複加算しない。
C++本体は同じ削除状態に至る経路もすべて探索している。
独立検算では状態ごとの計算を再利用するが、重複した各枝の経路数を足し戻して照合する。
探索が未完了なら `completion.json` が作られず、分析プログラムは未完了の出力を集計しない。

## 分類と集計

各問題を提案側ごとに以下へ分類する。

- `all_stable`: 全終点が元の問題に対してUR安定。
- `mixed`: 安定な終点と不安定な終点がある。
- `none_stable`: RDAの全終点が不安定。

初回DAで違反がなかった問題も含む全体の分類と、違反があって削除が必要だった問題だけの分類を両方示す。
同一入力における両提案側の分類の組み合わせも保存する。
経路数と、重複を除いた最終マッチング数は区別する。

経路数の比はIterated DAの乱択成功確率ではない。
`none_stable` はRDAの到達範囲に安定な終点がないという意味で、市場にUR安定マッチングが存在しないことの証明ではない。
ランダム標本についての実験であり、5×8の全選好を列挙したものでもない。

## 実行方法

`MatchingPrograms` を作業ディレクトリにする。

```sh
cmake -S . -B /tmp/rda-batch-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rda-batch-build --target run_rda_batch test_rda_batch test_algorithms_recursive_DA -j4
ctest --test-dir /tmp/rda-batch-build -R 'RDABatch|RecursiveDA' --output-on-failure

# 出力先は存在しないディレクトリを指定。
/tmp/rda-batch-build/experiments/universal_replacement/run_rda_batch \
  --trials 100 --seed 20261021 --output /tmp/rda-pilot
python3 experiments/universal_replacement/analyze_rda_batch.py /tmp/rda-pilot

/tmp/rda-batch-build/experiments/universal_replacement/run_rda_batch \
  --trials 1000 --seed 20261021 --output /tmp/rda-main
python3 experiments/universal_replacement/analyze_rda_batch.py /tmp/rda-main --pilot /tmp/rda-pilot

# 0始まりのtrial 964の入力・集計・証拠経路だけを再生成する例。
/tmp/rda-batch-build/experiments/universal_replacement/run_rda_batch \
  --trials 1 --start-trial 964 --seed 20261021 --output /tmp/rda-trial964
python3 experiments/universal_replacement/analyze_rda_batch.py /tmp/rda-trial964
```

実際のビルドではローカルに保存済みのGoogleTestソースを指定し、ネットワーク取得を行わずにビルドした。
既存の `recursive_*_proposing_DA_algorithm` をそのまま呼ぶため、実験用に別の削除規則を実装していない。
既存の1問題用の図生成スクリプトは呼び出していない。

保存済みの本実験を再検算する場合：

```sh
python3 experiments/universal_replacement/analyze_rda_batch.py \
  experiments/universal_replacement/recorded_results/rda_batch_20261021_seed20261021/main \
  --pilot experiments/universal_replacement/recorded_results/rda_batch_20261021_seed20261021/pilot
```

## 出力ファイル

| ファイル | 内容 |
|---|---|
| `config.json` | 件数・seed・選好の生成範囲・探索条件 |
| `completion.json` | 全件完了の記録・ノード総数・所要時間 |
| `instances.jsonl` | 全問題の入力と両提案側の集計、安定/不安定な証拠経路 |
| `profiles.csv` | 1行が1問題・1提案側の集計 |
| `summary.csv` | 選好条件・提案側ごとの3分類と、初期違反ありに限定した件数 |
| `by_initial_conflicts.csv` | 初回DAの違反ペア数による分類 |
| `joint_categories.csv` | 同じ問題の企業告白型と個人告白型の分類の組み合わせ |
| `firm_counterexamples.jsonl` | 企業告白型に不安定な経路があった全問題の入力・出力 |
| `representative_cases.json` | 各条件・提案側・分類のうち探索ノード数が最小の代表例 |
| `counterexamples.md` | 企業側の具体的な反例。空席への追加逸脱と満員企業への置換逸脱を掲載 |
| `report.md` | 人が読むための集計と結果の解釈 |
| `verification.json` | 独立検算した問題数・探索数・証拠経路数 |
| `source_sha256.json` | 関連する実装・検算コードのハッシュ |

JSONの企業・労働者ラベルは1始まり、未マッチは0。
試行番号・ノードID・違反ペアの選択インデックスは0始まり。
証拠経路には選択したペア、削除した側、残した相方、削除前/直後の全割当、相方の最終所属が含まれる。
