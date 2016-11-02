#ifndef CRETE_FACTORY_H
#define CRETE_FACTORY_H

#include <boost/function.hpp>

#include <map>

namespace crete
{

template
<
    typename Key,
    typename T
>
class Factory
{
public:
    typedef boost::function<T()> CreatorFunctor;
    typedef std::map<Key, CreatorFunctor> FactoryMap;

public:
    void insert(const Key& key, const CreatorFunctor& creator);
    T create(const Key& key);

protected:
private:
     FactoryMap mapping_;
};

template
<
    typename Key,
    typename T
>
void Factory<Key, T>::insert(const Key& key, const CreatorFunctor& creator)
{
    if(!mapping_.insert(std::make_pair(key, creator)).second)
        throw std::runtime_error("crete::Factory - failed to register creator function");
}

template
<
    typename Key,
    typename T
>
T Factory<Key, T>::create(const Key& key)
{
    typename FactoryMap::iterator it = mapping_.find(key);

    if(it == mapping_.end())
        throw std::runtime_error("crete::Factory - failed to find entry for key");

    return it->second();
}

} // namespace crete

#endif // CRETE_FACTORY_H
