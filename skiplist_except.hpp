#ifndef SKIPLIST_EXCEPT_HPP
#define SKIPLIST_EXCEPT_HPP

#include <exception>
#include <string>
#include <utility>

// Базовый класс исключений SkipList.
class SkipListException : public std::exception {
public:
    SkipListException() : msg_("SkipList: unspecified error") {}
    explicit SkipListException(std::string msg) : msg_(std::move(msg)) {}

    const char* what() const noexcept override { return msg_.c_str(); }

protected:
    std::string msg_;
};

// Бросается при попытке найти элемент, которого нет в контейнере.
class ElementNotFoundException : public SkipListException {
public:
    ElementNotFoundException()
        : SkipListException("SkipList: element not found") {}
    explicit ElementNotFoundException(const std::string& detail)
        : SkipListException("SkipList: element not found (" + detail + ")") {}
};

// Бросается при выходе индекса за допустимые границы (например, в kth(k)
// при k >= size()).
class IndexOutOfRangeException : public SkipListException {
public:
    IndexOutOfRangeException()
        : SkipListException("SkipList: index out of range") {}
    explicit IndexOutOfRangeException(const std::string& detail)
        : SkipListException("SkipList: index out of range (" + detail + ")") {}
};

#endif // SKIPLIST_EXCEPT_HPP
