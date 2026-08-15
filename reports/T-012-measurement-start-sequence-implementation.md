# T-012 計測開始シーケンス文書化レポート

## 対象

- 起動から初回UWB測距開始までのシーケンス
- マスター更新後のNTP再同期
- 現在実装済みの距離集約範囲

## 変更内容

- `docs/sequential-ranging-time-sync.md`へ、NVS・通信初期化、NodeStatus収集、初回500ms待機、最小TAG IDのマスター更新、session生成、全非マスターノードとのNTP同期、最小ANCHORへの`RangeControl`、最初のUWB測距開始を示すMermaidシーケンス図を追加した。
- マスターID、MAC、sessionの更新時は旧同期と旧roundを破棄し、全ANCHORと全フォロワーTAGが新マスターともう一度同期してから測距を再開することを明記した。
- `docs/feature-list.md`へ、距離結果はマスターTAGへ逐次集約・公開され、対象フォロワーTAGへ転送される一方、座標計算、EKF、永続履歴は未実装であることを追記した。

## 確認結果

- 既存の`TagMasterCoordinator`、`NtpTimeSynchronizer`、`SequentialRangingController`の実装順序と図を照合した。
- マスター更新後の再同期完了前に`StartMasterRound()`が開始されないことをコードと本文で確認した。
- Markdown code fence、見出し番号、末尾空白、差分形式を確認した。
- 製品コードとテストコードは変更していないため、buildとnative testは再実行していない。
- repository内に`tools/lint`、`package.json`、cspell設定がないため、Markdown focused/full lintはunsupportedであり成功扱いしていない。

## 残る範囲

- 座標計算とEKFは未実装である。
- 距離結果の長期履歴保存は未実装であり、現在は固定長FIFOから逐次取得する。
- 実機でのマスター交代、再同期、測距再開は引き続き実機検証対象である。
