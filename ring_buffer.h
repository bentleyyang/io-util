#ifndef RING_BUFFER_HPP
#define RING_BUFFER_HPP


/*
* ring_buffer_s.h
*/

#pragma once
#include <mutex>
#include <algorithm>
#include <cstring>
#include "spin_mutex.h"


/**
* \brief 线程安全的环形缓冲区
*/
class ring_buffer_s
{
public:
    explicit ring_buffer_s(size_t capacity)
        : front_(0)
            , rear_(0)
            , size_(0)
            , capacity_(capacity)
        {
            data_ = new uint8_t[capacity];
        }
    ring_buffer_s(ring_buffer_s &&) = delete;
    ring_buffer_s& operator=(ring_buffer_s&& other) = delete;

    ring_buffer_s(const ring_buffer_s &) = delete;
    ring_buffer_s& operator=(const ring_buffer_s& other) = delete;

    ~ring_buffer_s()
    {
        delete[] data_;
    }

    /**
     * \brief 获取缓冲区已用大小
     */
    inline size_t size() const
    {
        return size_;
    }

    /**
     * \brief 获取缓冲区容量
     */
    inline size_t capacity() const
    {
        return capacity_;
    }

    //not thread safe
    inline void clear()
    {
        front_=0;
        rear_=0;
        size_=0;
    }

    //not thread safe
    inline void set_read_pos(int64_t _pos)
    {

    }

    /**
     * \brief 写入数据
     * \param data 要写入的数据
     * \param bytes 要写入的数据的大小
     * \return 最终写入的数据的大小
     */
    inline size_t write(const void *data, size_t bytes)
    {
        if (bytes == 0) return 0;

        // 通过互斥量保证任意时刻，至多只有一个线程在写数据。
        std::lock_guard<std::mutex>lk_write(mut_write_);
        const auto capacity = capacity_;
        const auto bytes_to_write = std::min(bytes, capacity - size_);

        if (bytes_to_write == 0) return 0;

        // 一次性写入
        if (bytes_to_write <= capacity - rear_)
        {
            memcpy(data_ + rear_, data, bytes_to_write);
            rear_ += bytes_to_write;
            if (rear_ == capacity) rear_ = 0;
        }
        // 分两步进行
        else
        {
            const auto size_1 = capacity - rear_;
            memcpy(data_ + rear_, data, size_1);

            const auto size_2 = bytes_to_write - size_1;
            memcpy(data_, static_cast<const uint8_t*>(data) + size_1, size_2);
            rear_ = size_2;
        }

        // 通过自旋锁保证任意时刻，至多只有一个线程在改变 size_ .
        std::lock_guard<spin_mutex>lk(mut_);
        size_ += bytes_to_write;
        return bytes_to_write;
    }


    /**
     * \brief 读取数据
     * \param data 用来存放读取数据的buffer
     * \param bytes 要读取的数据大小
     * \return 最终获取到的数据的大小
     */
    inline size_t read(void *data, size_t bytes)
    {
        if (bytes == 0) return 0;

        // 通过互斥量保证任意时刻，至多只有一个线程在读数据。
        std::lock_guard<std::mutex>lk_read(mut_read_);

        const auto capacity = capacity_;
        const auto bytes_to_read = std::min(bytes, size_);

        if (bytes_to_read == 0) return 0;

        // 一次性读取
        if (bytes_to_read <= capacity - front_)
        {
            memcpy(data, data_ + front_, bytes_to_read);
            front_ += bytes_to_read;
            if (front_ == capacity) front_ = 0;
        }
        // 分两步进行
        else
        {
            const auto size_1 = capacity - front_;
            memcpy(data, data_ + front_, size_1);
            const auto size_2 = bytes_to_read - size_1;
            memcpy(static_cast<uint8_t*>(data) + size_1, data_, size_2);
            front_ = size_2;
        }

        // 通过自旋锁保证任意时刻，至多只有一个线程在改变 size_ .
        std::lock_guard<spin_mutex>lk(mut_);
        size_ -= bytes_to_read;
        return bytes_to_read;
    }

private:
    size_t front_, rear_, size_, capacity_;
    uint8_t *data_;
    mutable spin_mutex mut_;
    mutable std::mutex mut_read_;
    mutable std::mutex mut_write_;

};
#endif // RING_BUFFER_HPP
