//
// Created by loulfy on 12/07/2023.
//

#include "ler_sys.hpp"
#include "ler_log.hpp"

#ifdef _WIN32

#include <Windows.h>
#include <ShlObj.h>
#include <Wbemidl.h>
#include <comdef.h>

#else
#include <sys/sysinfo.h>
#include <unistd.h>
#include <pwd.h>
#endif

namespace ler
{
    std::string getHomeDir()
    {
#ifdef _WIN32
        wchar_t wide_path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, wide_path)))
        {
            char path[MAX_PATH];
            if (WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, path, MAX_PATH, nullptr, nullptr) > 0)
            {
                return path;
            }
        }
#else
        struct passwd* pw = getpwuid(getuid());
        if (pw != nullptr)
            return pw->pw_dir;
#endif

        return {};
    }

    unsigned int getRamCapacity()
    {
#ifdef _WIN32
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        if (GlobalMemoryStatusEx(&status))
        {
            DWORDLONG ramCapacity = status.ullTotalPhys;
            return ramCapacity / (1024ull * 1024ull);
        }
#else
        struct sysinfo info;
        if (sysinfo(&info) == 0)
        {
            long ramCapacity = info.totalram * info.mem_unit;
            return ramCapacity / (1024 * 1024);
        }
#endif
        return 0;
    }

    std::string getCpuName()
    {
#ifdef _WIN32
        HRESULT hres;

        // Step 1: Initialize COM
        hres = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hres))
        {
            log::error("Failed to initialize COM library. Error code: {}", hres);
            return {};
        }

        // Step 2: Initialize security
        hres = CoInitializeSecurity(
                nullptr,
                -1,
                nullptr,
                nullptr,
                RPC_C_AUTHN_LEVEL_DEFAULT,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr,
                EOAC_NONE,
                nullptr
        );
        if (FAILED(hres))
        {
            log::error("Failed to initialize security. Error code: {}", hres);
            CoUninitialize();
            return {};
        }

        // Step 3: Obtain the initial locator to WMI
        IWbemLocator* pLoc = nullptr;
        hres = CoCreateInstance(
                CLSID_WbemLocator,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_IWbemLocator,
                reinterpret_cast<LPVOID*>(&pLoc)
        );
        if (FAILED(hres))
        {
            log::error("Failed to create IWbemLocator object. Error code: {}", hres);
            CoUninitialize();
            return {};
        }

        // Step 4: Connect to WMI through the IWbemLocator interface
        IWbemServices* pSvc = nullptr;
        hres = pLoc->ConnectServer(
                _bstr_t(L"ROOT\\CIMV2"),
                nullptr,
                nullptr,
                nullptr,
                0,
                nullptr,
                nullptr,
                &pSvc
        );
        if (FAILED(hres))
        {
            log::error("Could not connect to WMI. Error code: ", hres);
            pLoc->Release();
            CoUninitialize();
            return {};
        }

        // Step 5: Set security levels on the proxy
        hres = CoSetProxyBlanket(
                pSvc,
                RPC_C_AUTHN_WINNT,
                RPC_C_AUTHZ_NONE,
                nullptr,
                RPC_C_AUTHN_LEVEL_CALL,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr,
                EOAC_NONE
        );
        if (FAILED(hres))
        {
            log::error("Could not set proxy blanket. Error code: {}", hres);
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            return {};
        }

        // Step 6: Query for the CPU name
        IEnumWbemClassObject* pEnumerator = nullptr;
        hres = pSvc->ExecQuery(
                _bstr_t("WQL"),
                _bstr_t("SELECT Name FROM Win32_Processor"),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                nullptr,
                &pEnumerator
        );
        if (FAILED(hres))
        {
            log::error("Failed to execute WQL query. Error code: {}", hres);
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            return {};
        }

        char cpuName[MAX_PATH];

        // Step 7: Retrieve CPU information from the query result
        IWbemClassObject* pclObj = nullptr;
        ULONG uReturn = 0;
        while (pEnumerator)
        {
            hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclObj, &uReturn);
            if (uReturn == 0)
                break;
            VARIANT vtProp;
            hres = pclObj->Get(L"Name", 0, &vtProp, nullptr, nullptr);
            if (SUCCEEDED(hres))
            {
                WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, cpuName, MAX_PATH, nullptr, nullptr);
                VariantClear(&vtProp);
            }
            pclObj->Release();
        }

        // Cleanup
        pSvc->Release();
        pLoc->Release();
        pEnumerator->Release();
        CoUninitialize();

        std::string trimmed(cpuName);
        trimmed.erase(trimmed.find_last_not_of(' ') + 1);
        return trimmed;
#else
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;

        if (cpuinfo.is_open())
        {
            while (std::getline(cpuinfo, line))
            {
                if (line.find("model name") != std::string::npos)
                {
                    return line.substr(line.find(":") + 2);
                }
            }
        }
#endif
        return {};
    }

    StdFileSystem::StdFileSystem(const fs::path& root) : m_root(root.lexically_normal())
    {
        if(m_root.empty())
            m_root = ".";
    }

    bool StdFileSystem::exists(const fs::path& path) const
    {
        return fs::exists(m_root / path);
    }

    Blob StdFileSystem::readFile(const fs::path& path)
    {
        // Open the stream to 'lock' the file.
        const fs::path name = m_root / path;
        std::ifstream f(name, std::ios::in | std::ios::binary);

        // Obtain the size of the file.
        std::error_code ec;
        const auto sz = static_cast<std::streamsize>(fs::file_size(name, ec));
        if(ec.value())
            throw std::runtime_error("File Not Found: " + ec.message());

        // Create a buffer.
        Blob result(sz);
        //auto blob = std::make_shared<Blobi>();
        //blob->data.resize(sz);

        // Read the whole file into the buffer.
        f.read(result.data(), sz);
        //f.read(blob->data.data(), sz);

        return result;
    }

    void StdFileSystem::enumerates(std::vector<fs::path>& entries)
    {
        for(const auto& entry : fs::recursive_directory_iterator(m_root))
        {
            std::string relative = entry.path().lexically_relative(m_root).generic_string();
            if(entry.is_regular_file())
                entries.emplace_back(relative);
        }
    }

    fs::file_time_type StdFileSystem::last_write_time(const fs::path& path)
    {
        return fs::last_write_time(m_root / path);
    }

    fs::path StdFileSystem::format_hint(const fs::path& path)
    {
        return path.extension();
    }

    void FileSystemService::mount(uint8_t tag, const FileSystemPtr& fs)
    {
        std::unique_lock lock(m_mutex);
        m_mountPoints.emplace(tag, fs);
    }

    const FileSystemPtr& FileSystemService::fileSystem(uint8_t tag) const
    {
        std::shared_lock lock(m_mutex);
        return m_mountPoints.at(tag);
    }

    FileSystemService& FileSystemService::Get()
    {
        static FileSystemService service;
        return std::ref(service);
    }

    const FileSystemPtr& FileSystemService::Get(uint8_t tag)
    {
        return Get().fileSystem(tag);
    }

    bool FileSystemService::exists(uint8_t tag, const fs::path& path)
    {
        std::shared_lock lock(m_mutex);
        if(m_mountPoints.contains(tag))
            return m_mountPoints[tag]->exists(path);
        return false;
    }

    Blob FileSystemService::readFile(uint8_t tag, const fs::path& path)
    {
        std::shared_lock lock(m_mutex);
        if(m_mountPoints.contains(tag))
        {
            FileSystemPtr& fs = m_mountPoints.at(tag);
            if(fs->exists(path))
                return fs->readFile(path);
        }
        return {};
    }

    void FileSystemService::enumerates(uint8_t tag, std::vector<fs::path>& entries)
    {
        std::shared_lock lock(m_mutex);
        if(m_mountPoints.contains(tag))
        {
            m_mountPoints[tag]->enumerates(entries);
        }
    }

    fs::file_time_type FileSystemService::last_write_time(uint8_t tag, const fs::path& path)
    {
        std::shared_lock lock(m_mutex);
        if(m_mountPoints.contains(tag))
        {
            FileSystemPtr& fs = m_mountPoints.at(tag);
            if(fs->exists(path))
                return fs->last_write_time(path);
        }
        return {};
    }
}