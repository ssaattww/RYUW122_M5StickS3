#pragma once

#include <cstddef>
#include <vector>

#include "NtShell.h"
#include "NvsPreferenceStore.h"

/**
 * @brief 型情報付きNVSをNT-Shellから操作するコマンドを提供します。
 */
class PreferenceCommands
{
public:
    /**
     * @brief NVSストアを利用するコマンド群を生成します。
     *
     * @param store コマンドから操作するNVSストア
     */
    explicit PreferenceCommands(NvsPreferenceStore& store);

    /**
     * @brief NT-Shellへ登録するコマンド一覧を取得します。
     *
     * @return NT-Shellコマンド一覧
     */
    const std::vector<NtShell::Command>& GetCommands() const;

private:
    /**
     * @brief 一覧出力で使用するコンテキストを保持します。
     */
    struct ListContext
    {
        Stream* m_output = nullptr;
        PreferenceCommands* m_commands = nullptr;
        size_t m_outputItemCount = 0;
        size_t m_valueErrorCount = 0;
    };

    /**
     * @brief NT-Shellのコールバックを対象インスタンスへ転送します。
     *
     * @param output コマンド結果の出力先
     * @param argc コマンド名を含む引数の個数
     * @param argv コマンド名を先頭に格納した引数配列
     * @param context PreferenceCommandsインスタンス
     */
    static void CommandPreference(
        Stream& output,
        int argc,
        char* argv[],
        void* context);

    /**
     * @brief NVS一覧項目をコマンド出力へ変換します。
     *
     * @param entry 一覧項目
     * @param context 一覧出力コンテキスト
     */
    static void VisitListEntry(const NvsEntryInfo& entry, void* context);

    /**
     * @brief prefコマンドのサブコマンドを解析して実行します。
     *
     * @param output コマンド結果の出力先
     * @param argc コマンド名を含む引数の個数
     * @param argv コマンド名を先頭に格納した引数配列
     */
    void Execute(Stream& output, int argc, char* argv[]);

    /**
     * @brief 指定された型で設定値を読み出して表示します。
     *
     * @param output コマンド結果の出力先
     * @param typeName 読み出す値の型名
     * @param key 読み出すキー
     */
    void GetValue(Stream& output, const char* typeName, const char* key);

    /**
     * @brief 指定された型で設定値を保存します。
     *
     * @param output コマンド結果の出力先
     * @param typeName 保存する値の型名
     * @param key 保存するキー
     * @param value 保存する文字列表現
     */
    void SetValue(
        Stream& output,
        const char* typeName,
        const char* key,
        const String& value);

    /**
     * @brief 現在のNVS名前空間に保存されている設定値を一覧表示します。
     *
     * @param output コマンド結果の出力先
     */
    void ListValues(Stream& output);

    /**
     * @brief 一覧の正常項目を読み出して出力します。
     *
     * @param output コマンド結果の出力先
     * @param entry 出力する項目
     * @return 値の読み出し結果
     */
    EnNvsResult WriteListValue(Stream& output, const NvsEntryInfo& entry);

    /**
     * @brief コマンドの使用方法を表示します。
     *
     * @param output コマンド結果の出力先
     */
    void PrintHelp(Stream& output) const;

    /**
     * @brief NVSストアを利用できる状態か検査します。
     *
     * @param output エラーの出力先
     * @return 利用できる場合はtrue、それ以外はfalse
     */
    bool EnsureStarted(Stream& output) const;

    /**
     * @brief NVSキーが有効か検査します。
     *
     * @param output エラーの出力先
     * @param key 検査するキー
     * @return 有効な場合はtrue、それ以外はfalse
     */
    bool ValidateKey(Stream& output, const char* key) const;

    NvsPreferenceStore& m_store;
    std::vector<NtShell::Command> m_commands;
};
