#ifndef IORDEREDSET_HPP
#define IORDEREDSET_HPP

#include <cstddef>

// Абстрактный интерфейс упорядоченного множества.
template <typename T>
class IOrderedSet {
public:
    virtual ~IOrderedSet() = default;

    virtual void        insert(const T& value)         = 0;
    virtual bool        erase(const T& value)          = 0;
    virtual bool        contains(const T& value) const = 0;
    virtual std::size_t size() const                   = 0;
    virtual bool        empty() const                  = 0;
    virtual void        clear()                        = 0;
};

#endif // IORDEREDSET_HPP
