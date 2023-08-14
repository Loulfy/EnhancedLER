//
// Created by loria on 12/07/2023.
//

#ifndef LER_SYS_HPP
#define LER_SYS_HPP

#include <span>
#include <string>
#include <vector>
#include <ranges>
#include <bitset>
#include <variant>
#include <fstream>
#include <typeindex>
#include <filesystem>
#include <shared_mutex>
#include <unordered_map>
#include <BS_thread_pool.hpp>
#include <entt/locator/locator.hpp>
#include <entt/signal/dispatcher.hpp>
namespace fs = std::filesystem;

#include "mpsc.hpp"

namespace ler
{
    std::string getHomeDir();
    std::string getCpuName();
    unsigned int getRamCapacity();

    static const fs::path ASSETS_DIR = fs::path(PROJECT_DIR) / "assets";
    static const fs::path CACHED_DIR = fs::path("cached");

    static constexpr uint32_t C16Mio = 16 * 1024 * 1024;
    static constexpr uint32_t C32Mio = 32 * 1024 * 1024;
    static constexpr uint32_t C64Mio = 64 * 1024 * 1024;
    static constexpr uint32_t C128Mio = 128 * 1024 * 1024;

    class Async
    {
    public:

        static BS::thread_pool& GetPool()
        {
            static Async async;
            return std::ref(async.m_pool);
        }

    private:

        Async() = default;
        BS::thread_pool m_pool;
    };

    template<class Request>
    class AsyncQueue
    {
    public:

        //using Request = std::variant<SubmitTexture>;

        static AsyncQueue& Get()
        {
            static AsyncQueue async;
            return std::ref(async);
        }

        static void Commit(const Request& req)
        {
            Get().m_queue.enqueue(req);
        }

        template<class T>
        static void Update(T& visitor)
        {
            Request req;
            while (Get().m_queue.dequeue(req))
            {
                std::visit(visitor, req);
            }
        }

    private:

        AsyncQueue() = default;
        MpscQueue<Request> m_queue;
    };

    class Event
    {
    public:

        static entt::dispatcher& GetDispatcher()
        {
            static Event mgr;
            return std::ref(mgr.m_dispatcher);
        }

    private:

        Event() = default;
        entt::dispatcher m_dispatcher;
    };

    enum FsTag
    {
        FsTag_Default = 0,
        FsTag_Cache = 1,
        FsTag_Assets = 2,
        FsTag_Assimp = 3
    };

    using Blob = std::vector<char>;

    class IFileSystem
    {
    public:

        virtual ~IFileSystem() = default;
        virtual Blob readFile(const fs::path& path) = 0;
        [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
        virtual void enumerates(std::vector<fs::path>& entries) = 0;
        [[nodiscard]] virtual fs::file_time_type last_write_time(const fs::path& path) = 0;
        [[nodiscard]] virtual fs::path format_hint(const fs::path& path) = 0;
    };

    using FileSystemPtr = std::shared_ptr<IFileSystem>;

    template <typename Derived>
    class FileSystem : public IFileSystem
    {
    public:

        static FileSystemPtr Create(const fs::path& path) { return std::make_shared<Derived>(path); }
    };

    class StdFileSystem : public FileSystem<StdFileSystem>
    {
    public:

        explicit StdFileSystem(const fs::path& root);
        Blob readFile(const fs::path& path) override;
        [[nodiscard]] bool exists(const fs::path& path) const override;
        void enumerates(std::vector<fs::path>& entries) override;
        [[nodiscard]] fs::file_time_type last_write_time(const fs::path& path) override;
        fs::path format_hint(const fs::path& path) override;

    private:

        fs::path m_root;
    };

    class FileSystemService
    {
    public:

        void mount(uint8_t tag, const FileSystemPtr& fs);
        [[nodiscard]] const FileSystemPtr& fileSystem(uint8_t tag) const;
        static FileSystemService& Get();
        static const FileSystemPtr& Get(uint8_t tag);

        Blob readFile(uint8_t tag, const fs::path& path);
        [[nodiscard]] bool exists(uint8_t tag, const fs::path& path);
        void enumerates(uint8_t tag, std::vector<fs::path>& entries);
        [[nodiscard]] fs::file_time_type last_write_time(uint8_t tag, const fs::path& path);

    private:

        FileSystemService() = default;
        std::unordered_map<uint8_t,FileSystemPtr> m_mountPoints;
        mutable std::shared_mutex m_mutex;
    };
}

#endif //LER_SYS_HPP
