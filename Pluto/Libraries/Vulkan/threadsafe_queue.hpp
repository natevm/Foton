#include <exception>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#ifndef THREADSAFE_QUEUE_HPP_INCLUDED
#define THREADSAFE_QUEUE_HPP_INCLUDED

struct empty_queue : std::exception
{
    virtual const char * what() const throw()
    {
        return "The Queue is Empty.";
    };
};

template <typename T>
class threadsafe_queue
{
private:
    std::queue<T> data;
    mutable std::mutex m;
public:
    threadsafe_queue() {};
    threadsafe_queue(const threadsafe_queue& other);

    threadsafe_queue& operator= (const threadsafe_queue&) = delete;

    void push(T new_value);

    T pop();

    void pop(T& value);

    bool empty();

    size_t size();
};

#include "./threadsafe_queue.tpp"

#endif // THREADSAFE_QUEUE_HPP_INCLUDED