# 再実行型doctor / firm proposing DAの全経路調査

[doctor版の結論・全経路のMermaid分岐図](da_paths_index.md)

[firm版の結論・全経路のMermaid分岐図・doctor版との比較](firm_da_paths_index.md)

同じ問題に対する再実行型firm版は、**全3経路がUR安定**で、うち1経路では8人全員がマッチした。
firm版の詳細は [01](firm_path_01.md)、[02](firm_path_02.md)、[03](firm_path_03.md) を参照。

添付画像の5企業・8個人問題について、今回追加した
`iterated_doctor_proposing_DA_algorithm` のあらゆるランダム選択経路を探索した。
**全4経路・4種類の最終マッチングすべてがUR不安定だった。**
各経路は [01](doctor_path_01.md)、[02](doctor_path_02.md)、
[03](doctor_path_03.md)、[04](doctor_path_04.md) に記録した。

## 対象となる仕様

1. 同僚選好を無視して、通常のdoctor proposing DAを終了まで実行する。
2. 同じ企業に禁止同僚ペアがなければ終了する。
3. 全禁止ペアから1組を選び、両端点の片方をその企業の選好から削除する。
4. 更新した企業選好を累積保持し、全員未マッチ・提案履歴なしからDAを再実行する。

firm版では手順1の通常DAをfirm proposingに置き換える。他の削除・初期化・判定規則は同じ。

判定は削除前の**元の選好**に対するuniversal-replacement安定性。
2026-10-14以降の実装は、企業が満員の場合の置換blockingに加えて空席への追加blockingも含む。
このフォルダの修正前に保存・生成した報告は空席blockingを除外した旧判定の記録であり、現在の判定で再実行・再検算する必要がある。
従来のラウンドごとの改良DAの資料は [modified_docs](../modified_docs/da_paths_index.md) にある。

## 再実行

`programs` を作業ディレクトリにする。

```sh
cmake -S . -B build
cmake --build build --target exhaustive_da_paths_5x8 -j4
./build/experiments/universal_replacement/exhaustive_da_paths_5x8 \
  --iterated-doctor \
  --output experiments/universal_replacement/iterated_docs/recorded_results
```

出力に表示される `iterated_doctor_paths_<番号>` のディレクトリを、次のコマンドに渡す。
保存済みの今回の実行データを再検算する場合：

```sh
python3 experiments/universal_replacement/iterated_docs/render_iterated_doctor_paths.py \
  experiments/universal_replacement/iterated_docs/recorded_results/iterated_doctor_paths_1791367650792
```

firm版のC++全経路探索：

```sh
./build/experiments/universal_replacement/exhaustive_da_paths_5x8 \
  --iterated-firm \
  --output experiments/universal_replacement/iterated_docs/recorded_results
```

保存済みfirm版の全経路を独立実装で再検算し、資料を再生成：

```sh
python3 experiments/universal_replacement/iterated_docs/render_iterated_firm_paths.py \
  experiments/universal_replacement/iterated_docs/recorded_results/iterated_firm_paths_1791368334862
```

新規実行の再検算では、その実行で表示されたディレクトリ名に置き換える。
firmのPythonモデルは、全企業の提案順序と個人の移動による旧企業の提案再開を別実装し、
共有する全分岐探索・データ照合処理に渡す。doctorの計算結果をfirmの結果として流用しない。

C++は本番アルゴリズムの `DARandomChoiceSelector` と `enumerate_choice_paths` を使い、
全ペア選択・両削除対象に分岐する。PythonはC++を呼ばず、標準DAと削除・再実行を
別実装して全経路を生成する。各選択時点と最終結果、blocking pair、CSVの一致を確認した後に
Markdown・Mermaid・全提案ログを生成する。外部Pythonライブラリは不要。

今回の報告はこの固定問題の実行結果に特化している。再検算で経路数や結論が変わると、
スクリプトは報告を更新する前にエラーにして、古い結論を新しい結果として出力しない。

## 保存データ

[今回の実行ディレクトリ](recorded_results/iterated_doctor_paths_1791367650792/summary.txt) に以下を保存した。

| ファイル | 内容 |
|---|---|
| `problem.json` | 元の定員・順位・全選好行列 |
| `summary.txt` | C++の全経路集計と仮説判定 |
| `iterated_doctor_paths.jsonl` | 各選択時点の割当と全経路の終点・blocking pair |
| `iterated_doctor_paths.csv` | 全経路の削除履歴・最終割当・blocking pair |
| `verification.json` | 独立検算の集計 |
| `doctor_da_traces.json` | Python独立計算による全DA・全提案・累積削除 |

[firm版の実行ディレクトリ](recorded_results/iterated_firm_paths_1791368334862/summary.txt) には、
同じ内容の `problem.json`、`summary.txt`、`iterated_firm_paths.jsonl`、
`iterated_firm_paths.csv`、`verification.json`、`firm_da_traces.json` を保存した。

企業・個人の番号は1始まり。割当ベクトルはw1〜w8の順で、`0`は未マッチ。
選択インデックスだけは0始まり。`round`は完全なDAの実行回数を表す。

全経路は深さの異なる分岐木の葉なので、経路数の比を乱択の確率としては扱わない。
