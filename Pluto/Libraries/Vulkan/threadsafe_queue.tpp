#include "./threadsafe_queue.hpp"

template <typename T>
threadsafe_queue<T>::threadsafe_queue(const threadsafe_queue& other)
{
    std::lock_guard<std::mutex> lock(other.m);
    threadsafe_queue<T>::data = other.data;
}

template <typename T>
void threadsafe_queue<T>::push(T new_value)
{
    std::lock_guard<std::mutex> lock(threadsafe_queue<T>::m);
    threadsafe_queue::data.push(new_value);
}

template <typename T>
T threadsafe_queue<T>::pop()
{
    std::lock_guard<std::mutex> lock(threadsafe_queue<T>::m);
    if(data.empty()) throw empty_queue();
    T res = threadsafe_queue<T>::data.front();
    threadsafe_queue<T>::data.pop();
    return res;
}

template <typename T>
void threadsafe_queue<T>::pop(T& value)
{
    std::lock_guard<std::mutex> lock(threadsafe_queue::m);
    if(data.empty()) throw empty_queue();
    value = threadsafe_queue::data.front();
    threadsafe_queue::data.pop();
}

template <typename T>
size_t threadsafe_queue<T>::size()
{
    std::lock_guard<std::mutex> lock(threadsafe_queue<T>::m);
    return threadsafe_queue<T>::data.size();
}

template <typename T>
bool threadsafe_queue<T>::empty()
{
    std::lock_guard<std::mutex> lock(threadsafe_queue<T>::m);
    return threadsafe_queue<T>::data.empty();
}