#ifndef CRETE_REGISTRY
#define CRETE_REGISTRY

#include <map>
#include <string>

namespace crete
{

template <typename T>
class Registry
{
public:
    static T& get()
    {
        static T t_;

        return t_;
    }
};

template <typename T>
class RegistryMap
{
public:
    static T& get(std::string key)
    {
        static std::map<std::string, T> map_;

        typename std::map<std::string, T>::iterator res = map_.find(key);
        if(res != map_.end())
            return res->second;

        assert(map_.insert(std::make_pair(key, T())).second);

        res = map_.find(key);

        return res->second;
    }    
};

}

#endif // CRETE_REGISTRY
