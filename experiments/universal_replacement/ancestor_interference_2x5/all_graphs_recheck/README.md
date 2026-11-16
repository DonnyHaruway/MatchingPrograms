# 全分岐失敗という追加条件の再検査

2026-11-04。企業2・個人5、定員各2、個人告白型Recursive DA、厳密選好、企業によらないbinary同僚拒否。

**この範囲では、どの削除分岐を選んでも元問題のUR安定解を導出しない入力は0件だった。** 2段階の干渉を条件に絞った検査だけでなく、その条件を外した全入力も、独立したPython実装による経路証明付きで確認した。

したがって、この設定を維持する限り、「子の削除が祖先に干渉する2段階の経路」と「全分岐が失敗」を同時に満たす例は構成できない。これは有限範囲についての計算による確認であり、企業数・個人数・定員・無差別などを変えた一般的不可能性定理ではない。

## 全入力での結果：干渉条件も深さ制限も課さない

| 選好の範囲 | 正規化選好数 | 各選好での禁止集合数 | 全入力数 | 全分岐失敗 |
|---|---:|---:|---:|---:|
| 全員相互に受入可能 | 3,840 | 1,024 | 3,932,160 | **0** |
| 受入不可も許す | 375,000 | 1,024 | 384,000,000 | **0** |

前者は後者に含まれる。入力数は個人名を正規化した選好と禁止集合の組合せ数であり、すべての意味的同型類をさらに除いた数ではない。

各入力について、実行可能な削除分岐をたどって、元問題でUR安定な終点に達する経路が少なくとも1本存在することを確認した。他の枝の深さは制限していない。したがって「全探索木が2階層以内」という条件を追加しても、結果は0件のままである。

主な結果ファイル：

- [C++：全員受入可能](global_complete_summary.json)
- [C++：受入不可も許す](global_incomplete_summary.json)
- [独立Python：全員受入可能](global_complete_independent_verification.json)
- [独立Python：受入不可も許す](global_incomplete_independent_verification.json)

## 2段階の干渉を持つ全禁止集合の再検査

前回の保存済み代表に限定せず、同じ経路に対して条件を満たすすべての禁止集合を調べ直した。代表集合の選択は行っていない。

以下を満たす経路を広い対象とした。

1. 元問題のDAで現れた禁止ペアから、企業・個人の組d1を削除する。
2. その子問題のDAで現れた禁止ペアから、別の企業・個人の組d2を削除する。
3. 2回削除後は同居する禁止ペアがなく、元問題ではd1がUR逸脱、d2はUR逸脱ではない。
4. d1は1回削除後の中間割当ではUR逸脱ではなく、子の削除によってUR逸脱になる。

終点の両企業満員、異なる企業での削除、子で新しく現れた禁止ペア、祖先の削除だけを戻した比較結果のUR安定性は、ここでは必須にしていない。前回の主例が満たした条件を含む、より広い範囲である。

| 選好の範囲 | 条件を満たす異なる「選好＋禁止集合」 | そのうち全分岐失敗 |
|---|---:|---:|
| 全員受入可能 | 462,816 | **0** |
| 受入不可も許す | 38,582,496 | **0** |

経路を区別すると、後者には42,679,648通りの「経路＋禁止集合」の組合せがある。同じ入力が複数経路を持つ場合は、分岐木の検査は1回だけ行う。

- [C++集計](incomplete_summary.json)
- [全対象入力の網羅性と安定経路の独立検算](incomplete_independent_verification.json)

## 「代表だけ保存した」前回との違い

前回は、ある干渉経路を満たす禁止集合を複数調べた後、その経路について代表を1つ保存し、保存した入力の分岐木を調べていた。そのため、代表に選ばれない禁止集合の全分岐失敗の可否は未確認だった。

今回は、その漏れを解消したうえで、さらに全入力の検査に進んだ。全入力検査では1本の安定経路が複数の禁止集合で共通して使えることを利用しているが、**各経路が有効な禁止集合を1,024ビットの集合として明示的に計算し、その和集合が全1,024集合を覆うことを、各選好について独立検算している。** 単に1つの禁止集合を代表として、ほかの集合にも結果を当てはめる処理ではない。

## 安定経路の証明と独立検算

ある削除経路と、その経路に沿った各DAの割当を固定する。定員各2のこの設定では、その経路が有効で、終点が元問題でUR安定となる禁止集合Bを、次の条件で厳密に記述できる。

- 経路上で削除理由に選んだ各同居ペアはBに含まれる。
- 終点の各企業で同居するペアはBに含まれない。
- 同僚制約を無視すると元問題で逸脱できる各企業・個人の組について、終点の在籍者との禁止関係が少なくとも1つある。これによりuniversal replacementまたは空席への追加逸脱を防ぐ。

全員相互に受入可能な範囲では82,530本、受入不可を許す範囲では2,468,628本の安定経路を保存した。それぞれが使える禁止集合の和集合で、全入力を覆った。経路のない入力や、覆われない禁止集合はない。

C++側は逐次提案のDAと削除集合を用いた探索を行う。Python側はラウンドごとの同時提案DAを独立に実行し、保存経路の各削除が現に同居する禁止ペアに対する削除であること、終点の実行可能性と元問題のUR安定性、全禁止集合の被覆を確認する。C++側の探索木探索をPythonで呼び出して検算する方式ではない。

対象を絞った3,858万件についても、1件ごとの安定経路を別形式で保存し、Pythonで対象入力の集合を再構成して、証明の存在する入力集合と完全一致することを確認した。

この検査で必要なのは「安定な終点への経路が1本でもあるか」である。C++の探索では1本見つかった時点で、その入力について全分岐失敗を否定できるので探索を打ち切る。見つからない場合は全削除枝を調べる設計だが、今回そのような入力はなかった。

## 入力範囲の網羅性

厳密な受入可能順位と未マッチを考える。片側が拒否する組は相互の受入不可にまとめてよく、それらを個人側のリストへ移す。企業の残る順位を完全な厳密順位へ拡張して、個人名をf1の順位に沿って付け替える。

これによりf1の順位を固定でき、f2は5!=120通り。各個人のリストは「f1>f2、f2>f1、f1のみ、f2のみ、両方不可」の5通りなので120×5⁵=375,000通り。全員受入可能なら120×2⁵=3,840通り。共通禁止集合は5人の10組の有無なので2¹⁰=1,024通りである。

定員は各2、選好は受入可能な企業間・未マッチとの間で厳密、同僚制約は企業によらないbinary拒否である。その他のモデルへの一般化は、この結果からは行わない。

## 証明ファイル

すべてXZで圧縮し、元バイト列とのSHA-256一致を解凍して確認した。[ファイルサイズとハッシュ](certificate_files.json)

- [全入力用：全員受入可能](global_complete_graph_cover.bin.xz)
- [全入力用：受入不可を許す](global_incomplete_graph_cover.bin.xz)
- [干渉条件で絞った入力用：全員受入可能](complete_stable_paths.bin.xz)
- [干渉条件で絞った入力用：受入不可を許す](incomplete_stable_paths.bin.xz)

全入力用の先頭4バイトは`URG1`。各選好のレコードは、選好ID（u32）、経路数（u16）、失敗グラフ数（u16）、経路ごとの削除数（u8）と削除列（5バイト）である。今回の失敗グラフ数はすべて0。整数はlittle-endian。削除列は企業番号×5＋個人番号（ともに0始まり）を4ビットずつ格納し、先頭削除が最下位である。

選好IDはf2の順列の辞書順番号×base⁵＋個人リストのコード。コードの最下位桁がw1で、baseは全員受入可能なら2、受入不可を許すなら5。各リストの型の順序は上述の5通りである。

## 再現方法

以下は親ディレクトリ `ancestor_interference_2x5` で実行する。

```sh
c++ -std=c++17 -O3 -Wall -Wextra recheck_all_graphs.cpp -o /tmp/ancestor_recheck
/tmp/ancestor_recheck complete all_graphs_recheck/complete
/tmp/ancestor_recheck incomplete all_graphs_recheck/incomplete
python3 verify_all_graph_certificates.py --mode complete --certificate all_graphs_recheck/complete_stable_paths.bin --compare all_graphs_recheck/complete_summary.json --output all_graphs_recheck/complete_independent_verification.json
python3 verify_all_graph_certificates.py --mode incomplete --certificate all_graphs_recheck/incomplete_stable_paths.bin --compare all_graphs_recheck/incomplete_summary.json --output all_graphs_recheck/incomplete_independent_verification.json

c++ -std=c++17 -O3 -Wall -Wextra recheck_entire_input_space.cpp -o /tmp/ancestor_entire_input_space
/tmp/ancestor_entire_input_space complete all_graphs_recheck/global_complete
/tmp/ancestor_entire_input_space incomplete all_graphs_recheck/global_incomplete
python3 verify_entire_input_space.py --mode complete --certificate all_graphs_recheck/global_complete_graph_cover.bin --compare all_graphs_recheck/global_complete_summary.json --output all_graphs_recheck/global_complete_independent_verification.json
python3 verify_entire_input_space.py --mode incomplete --certificate all_graphs_recheck/global_incomplete_graph_cover.bin --compare all_graphs_recheck/global_incomplete_summary.json --output all_graphs_recheck/global_incomplete_independent_verification.json
```

保存済みの圧縮証明だけを検算する場合は、`--certificate`を対応する`.bin.xz`に変える。生成用C++の再実行は不要である。

既存リポジトリのC++本体と既存Python検算器による、選択した12入力の全分岐木の照合結果も [別途保存](repository_sample_verification.json) した。
