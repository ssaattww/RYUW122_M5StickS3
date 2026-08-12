#pragma once

#include <cstddef>
#include <memory>
#include <vector>

class Stream;

/**
 * @brief NT-Shellを別スレッドで実行し、外部登録されたコマンドを処理します。
 * コマンドはStart()を呼び出す前に登録してください。
 */
class NtShell
{
public:
    /**
     * @brief 外部コマンドを処理する関数の型です。
     * ハンドラーはNT-Shellのスレッド上で呼び出されます。
     *
     * @param output コマンド結果の出力先
     * @param argc コマンド名を含む引数の個数
     * @param argv コマンド名を先頭に格納した引数配列
     * @param context 登録時に指定した任意のコンテキスト
     */
    using CommandHandler = void (*)(
        Stream& output,
        int argc,
        char* argv[],
        void* context);

    /**
     * @brief 外部から登録するコマンドの定義です。
     */
    struct Command
    {
        const char* m_name;
        const char* m_help;
        CommandHandler m_handler;
        void* m_context;
    };

    /**
     * @brief NT-Shellを指定したストリームへ接続します。
     *
     * @param serial 入出力に使用するストリーム
     */
    explicit NtShell(Stream& serial);

    /**
     * @brief NT-Shellの管理オブジェクトを破棄します。
     * Start()後は、アプリケーション終了までこのオブジェクトを保持してください。
     */
    ~NtShell();

    /**
     * @brief 外部コマンドを1件登録します。
     * コマンド名と説明は内部へコピーされます。
     *
     * @param command 登録するコマンド
     * @return 登録できた場合はtrue、名前やハンドラーが不正な場合はfalse
     */
    bool RegisterCommand(const Command& command);

    /**
     * @brief 外部コマンドをまとめて登録します。
     * 1件でも不正な定義や重複名がある場合は、どのコマンドも登録しません。
     *
     * @param commands 登録するコマンド一覧
     * @return 全コマンドを登録できた場合はtrue、それ以外はfalse
     */
    bool RegisterCommands(const std::vector<Command>& commands);

    /**
     * @brief NT-Shellの処理スレッドを開始します。
     * 2回目以降の呼び出しや、スレッドを開始できない場合は失敗します。
     *
     * @return 処理スレッドを開始できた場合はtrue、それ以外はfalse
     */
    bool Start();

private:
    /**
     * @brief 入出力先を持たない生成を禁止します。
     */
    NtShell() = delete;

    class NtShellThread;
    std::unique_ptr<NtShellThread> m_thread;
};
