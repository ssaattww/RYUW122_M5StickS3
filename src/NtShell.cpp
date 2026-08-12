#include "NtShell.h"

#include <Arduino.h>

#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "core/ntshell.h"
#include "core/ntconf.h"
#include "esp_pthread.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class NtShell::NtShellThread
{
public:
    /**
     * @brief NT-Shellの実行状態を初期化します。
     *
     * @param serial 入出力に使用するストリーム
     */
    explicit NtShellThread(Stream& serial)
        : m_serial(serial)
    {
    }

    /**
     * @brief 外部コマンドを1件登録します。
     *
     * @param command 登録するコマンド
     * @return 登録できた場合はtrue、それ以外はfalse
     */
    bool RegisterCommand(const Command& command)
    {
        if (m_started || !IsValidCommand(command))
        {
            return false;
        }

        AddCommand(command);
        return true;
    }

    /**
     * @brief 外部コマンドをまとめて登録します。
     * 登録前に全要素を検査するため、失敗時に一部だけが追加されることはありません。
     *
     * @param commands 登録するコマンド一覧
     * @return 全コマンドを登録できた場合はtrue、それ以外はfalse
     */
    bool RegisterCommands(const std::vector<Command>& commands)
    {
        if (m_started)
        {
            return false;
        }

        for (size_t index = 0; index < commands.size(); ++index)
        {
            if (!IsValidCommand(commands[index]))
            {
                return false;
            }

            for (size_t otherIndex = index + 1; otherIndex < commands.size(); ++otherIndex)
            {
                if (commands[otherIndex].m_name != nullptr &&
                    strcmp(commands[index].m_name, commands[otherIndex].m_name) == 0)
                {
                    return false;
                }
            }
        }

        m_commands.reserve(m_commands.size() + commands.size());
        for (const Command& command : commands)
        {
            AddCommand(command);
        }

        return true;
    }

    /**
     * @brief NT-Shellの処理スレッドを開始します。
     *
     * @return 処理スレッドを開始できた場合はtrue、それ以外はfalse
     */
    bool Start()
    {
        if (m_started)
        {
            return false;
        }

        esp_pthread_cfg_t config = esp_pthread_get_default_config();
        config.thread_name = "ntshell";
        config.stack_size = 4096;
        config.prio = 2;
        config.pin_to_core = tskNO_AFFINITY;

        if (esp_pthread_set_cfg(&config) != ESP_OK)
        {
            return false;
        }

        m_started = true;
        std::thread([this]()
        {
            Run();
        }).detach();

        return true;
    }

private:
    struct CommandEntry
    {
        std::string m_name;
        std::string m_help;
        CommandHandler m_handler;
        void* m_context;
    };

    /**
     * @brief NT-Shellからの読み取り要求を対象インスタンスへ転送します。
     *
     * @param buffer 読み取ったデータの格納先
     * @param count 読み取るバイト数
     * @param context NtShellThreadインスタンス
     * @return 読み取ったバイト数
     */
    static int ReadCallback(char* buffer, int count, void* context)
    {
        auto* self = static_cast<NtShellThread*>(context);
        return self->Read(buffer, count);
    }

    /**
     * @brief NT-Shellからの書き込み要求を対象インスタンスへ転送します。
     *
     * @param buffer 書き込むデータ
     * @param count 書き込むバイト数
     * @param context NtShellThreadインスタンス
     * @return 書き込んだバイト数
     */
    static int WriteCallback(const char* buffer, int count, void* context)
    {
        auto* self = static_cast<NtShellThread*>(context);
        return self->Write(buffer, count);
    }

    /**
     * @brief NT-Shellから受け取ったコマンド文字列を対象インスタンスへ転送します。
     *
     * @param text 実行するコマンド文字列
     * @param context NtShellThreadインスタンス
     * @return コマンド処理結果
     */
    static int CommandCallback(const char* text, void* context)
    {
        auto* self = static_cast<NtShellThread*>(context);
        return self->ExecuteCommand(text);
    }

    /**
     * @brief NT-Shellのブロッキング処理を専用スレッド上で実行します。
     */
    void Run()
    {
        ntshell_init(
            &m_shell,
            ReadCallback,
            WriteCallback,
            CommandCallback,
            this);
        ntshell_set_prompt(&m_shell, "> ");
        ntshell_execute(&m_shell);
    }

    /**
     * @brief 入力ストリームから指定されたバイト数を読み取ります。
     *
     * @param buffer 読み取ったデータの格納先
     * @param count 読み取るバイト数
     * @return 読み取ったバイト数
     */
    int Read(char* buffer, int count)
    {
        for (int index = 0; index < count; ++index)
        {
            int value = -1;
            while (value < 0)
            {
                value = m_serial.read();
                if (value < 0)
                {
                    vTaskDelay(1);
                }
            }

            buffer[index] = static_cast<char>(value);

            // 入力が連続している場合もIDLEタスクへ実行時間を渡します。
            vTaskDelay(1);
        }

        return count;
    }

    /**
     * @brief 出力ストリームへ指定されたデータを書き込みます。
     *
     * @param buffer 書き込むデータ
     * @param count 書き込むバイト数
     * @return 書き込んだバイト数
     */
    int Write(const char* buffer, int count)
    {
        return static_cast<int>(m_serial.write(
            reinterpret_cast<const uint8_t*>(buffer),
            static_cast<size_t>(count)));
    }

    /**
     * @brief コマンド文字列を引数へ分割し、対応する外部ハンドラーを呼び出します。
     *
     * @param text 実行するコマンド文字列
     * @return NT-Shellへ返す処理結果
     */
    int ExecuteCommand(const char* text)
    {
        constexpr size_t MaxLineLength = NTCONF_EDITOR_MAXLEN;
        constexpr int MaxArguments = NTCONF_EDITOR_MAXLEN / 2;

        if (text == nullptr)
        {
            return -1;
        }

        if (strlen(text) >= MaxLineLength)
        {
            Printf("ERROR command_too_long\r\n");
            return -1;
        }

        char line[MaxLineLength];
        strncpy(line, text, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';

        char* arguments[MaxArguments];
        int argumentCount = 0;
        char* savePointer = nullptr;
        char* token = strtok_r(line, " \t", &savePointer);

        while (token != nullptr && argumentCount < MaxArguments)
        {
            arguments[argumentCount++] = token;
            token = strtok_r(nullptr, " \t", &savePointer);
        }

        if (token != nullptr)
        {
            Printf("ERROR too_many_arguments\r\n");
            return -1;
        }

        if (argumentCount == 0)
        {
            return 0;
        }

        if (strcmp(arguments[0], "help") == 0)
        {
            PrintHelp();
            return 0;
        }

        for (const CommandEntry& command : m_commands)
        {
            if (command.m_name == arguments[0])
            {
                command.m_handler(
                    m_serial,
                    argumentCount,
                    arguments,
                    command.m_context);
                return 0;
            }
        }

        Printf("Unknown command: %s\r\n", arguments[0]);
        return 0;
    }

    /**
     * @brief 組み込みのhelpコマンドとして登録済みコマンド一覧を表示します。
     */
    void PrintHelp()
    {
        Printf("%-12s %s\r\n", "help", "Show command list");
        for (const CommandEntry& command : m_commands)
        {
            Printf(
                "%-12s %s\r\n",
                command.m_name.c_str(),
                command.m_help.c_str());
        }
    }

    /**
     * @brief コマンド定義の名前、重複、ハンドラーを検査します。
     *
     * @param command 検査するコマンド
     * @return 登録可能な場合はtrue、それ以外はfalse
     */
    bool IsValidCommand(const Command& command) const
    {
        if (command.m_name == nullptr ||
            command.m_name[0] == '\0' ||
            command.m_handler == nullptr ||
            strcmp(command.m_name, "help") == 0 ||
            strpbrk(command.m_name, " \t\r\n\v\f") != nullptr)
        {
            return false;
        }

        for (const CommandEntry& registeredCommand : m_commands)
        {
            if (registeredCommand.m_name == command.m_name)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief 検査済みのコマンド定義を内部一覧へコピーします。
     *
     * @param command 追加する検査済みコマンド
     */
    void AddCommand(const Command& command)
    {
        m_commands.push_back(
            {
                command.m_name,
                command.m_help == nullptr ? "" : command.m_help,
                command.m_handler,
                command.m_context,
            });
    }

    /**
     * @brief 書式付き文字列を出力ストリームへ書き込みます。
     *
     * @tparam Args 書式へ埋め込む値の型
     * @param format printf互換の書式文字列
     * @param args 書式へ埋め込む値
     */
    template <typename... Args>
    void Printf(const char* format, Args... args)
    {
        char buffer[128];
        const int length = snprintf(buffer, sizeof(buffer), format, args...);

        if (length > 0)
        {
            m_serial.write(
                reinterpret_cast<const uint8_t*>(buffer),
                static_cast<size_t>(min(
                    length,
                    static_cast<int>(sizeof(buffer) - 1))));
        }
    }

    Stream& m_serial;
    ntshell_t m_shell{};
    std::vector<CommandEntry> m_commands;
    bool m_started = false;
};

/**
 * @brief NT-Shellを指定したストリームへ接続します。
 *
 * @param serial 入出力に使用するストリーム
 */
NtShell::NtShell(Stream& serial)
    : m_thread(new NtShellThread(serial))
{
}

/**
 * @brief NT-Shellの管理オブジェクトを破棄します。
 */
NtShell::~NtShell() = default;

/**
 * @brief 外部コマンドを1件登録します。
 *
 * @param command 登録するコマンド
 * @return 登録できた場合はtrue、それ以外はfalse
 */
bool NtShell::RegisterCommand(const Command& command)
{
    return m_thread->RegisterCommand(command);
}

/**
 * @brief 外部コマンドをまとめて登録します。
 *
 * @param commands 登録するコマンド一覧
 * @return 全コマンドを登録できた場合はtrue、それ以外はfalse
 */
bool NtShell::RegisterCommands(const std::vector<Command>& commands)
{
    return m_thread->RegisterCommands(commands);
}

/**
 * @brief NT-Shellの処理スレッドを開始します。
 *
 * @return 処理スレッドを開始できた場合はtrue、それ以外はfalse
 */
bool NtShell::Start()
{
    return m_thread->Start();
}
