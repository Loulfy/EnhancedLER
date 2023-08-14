//
// Created by loulfy on 15/07/2023.
//

#ifndef LER_SVC_HPP
#define LER_SVC_HPP

#include "ler_dev.hpp"

namespace ler
{
    class IService
    {
    public:

        virtual ~IService() = default;
        virtual void init(LerDevicePtr device) = 0;
        virtual void update() = 0;
        virtual void destroy() = 0;
    };

    class Service
    {
    public:

        static Service& Instance()
        {
            static Service mgr;
            return std::ref(mgr);
        }

        template<typename T>
        static std::shared_ptr<T> Get()
        {
            auto& mgr = Instance();
            if(!mgr.m_services.contains(typeid(T)))
            {
                std::shared_ptr<IService> svc = std::make_shared<T>();
                svc->init(mgr.m_device.lock());
                mgr.m_services.insert({typeid(T), svc});
            }

            return std::static_pointer_cast<T>(mgr.m_services.at(typeid(T)));
        }

        static void Init(LerDevicePtr& device)
        {
            auto& mgr = Instance();
            mgr.m_device = device;
        }

        static void Update()
        {
            auto& mgr = Instance();
            for(const auto& entry : mgr.m_services)
                entry.second->update();
        }

        static void Destroy()
        {
            auto& mgr = Instance();
            for(const auto& entry : mgr.m_services)
                entry.second->destroy();
            mgr.m_services.clear();
        }

    private:

        Service() = default;
        std::weak_ptr<LerDevice> m_device;
        std::unordered_map<std::type_index,std::shared_ptr<IService>> m_services;
    };
}

#endif //LER_SVC_HPP
