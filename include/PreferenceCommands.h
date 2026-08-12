#pragma once

#include <Preferences.h>

#include <string>
#include <vector>

#include "NtShell.h"

/**
 * @brief ESP32のPreferencesをNT-Shellから確認・変更するコマンドを提供します。
 */
class PreferenceCommands
{
public:
    /**
     * @brief 指定されたNVS名前空間を使用するコマンド群を生成します。
     *
     * @param namespaceName 使用する値保存用NVS名前空間
     * @param metadataNamespaceName 使用する型情報保存用NVS名前空間
     */
    PreferenceCommands(
        const char* namespaceName,
        const char* metadataNamespaceName);

    /**
     * @brief Preferencesの使用を終了します。
     */
    ~PreferenceCommands();

    /**
     * @brief NVS名前空間を読み書き可能な状態で開きます。
     *
     * @return 名前空間を開けた場合はtrue、それ以外はfalse
     */
    bool Begin();

    /**
     * @brief NT-Shellへ登録するコマンド一覧を取得します。
     *
     * @return NT-Shellコマンド一覧
     */
    const std::vector<NtShell::Command>& GetCommands() const;

private:
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
     * @brief コマンドの使用方法を表示します。
     *
     * @param output コマンド結果の出力先
     */
    void PrintHelp(Stream& output) const;

    /**
     * @brief Preferencesを利用できる状態か検査します。
     *
     * @param output エラーの出力先
     * @return 利用できる場合はtrue、それ以外はfalse
     */
    bool EnsureStarted(Stream& output) const;

    /**
     * @brief NVSキーが有効な長さか検査します。
     *
     * @param output エラーの出力先
     * @param key 検査するキー
     * @return 有効な場合はtrue、それ以外はfalse
     */
    bool ValidateKey(Stream& output, const char* key) const;

    Preferences m_preferences;
    Preferences m_metadataPreferences;
    std::string m_namespace;
    std::string m_metadataNamespace;
    std::vector<NtShell::Command> m_commands;
    bool m_started = false;
};
