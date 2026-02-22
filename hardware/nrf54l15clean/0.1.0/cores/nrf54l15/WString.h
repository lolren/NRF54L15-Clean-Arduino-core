/*
 * Arduino WString - String class for Arduino
 *
 * Licensed under the Apache License 2.0
 */

#ifndef WString_h
#define WString_h

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class String {
public:
    String() { assign(""); }

    String(const char *value)
    {
        assign(value == nullptr ? "" : value);
    }

    String(char value)
    {
        char tmp[2] = {value, '\0'};
        assign(tmp);
    }

    String(unsigned char value)
    {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "%u", value);
        assign(tmp);
    }

    String(int value)
    {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%d", value);
        assign(tmp);
    }

    String(unsigned int value)
    {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%u", value);
        assign(tmp);
    }

    String(long value)
    {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%ld", value);
        assign(tmp);
    }

    String(unsigned long value)
    {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%lu", value);
        assign(tmp);
    }

    String(float value)
    {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%f", (double)value);
        assign(tmp);
    }

    String(double value)
    {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%f", value);
        assign(tmp);
    }

    String(const String &other)
    {
        assign(other.c_str());
    }

    String(String &&other) noexcept
        : _data(other._data), _length(other._length)
    {
        other._data = nullptr;
        other._length = 0;
    }

    ~String()
    {
        free(_data);
    }

    String &operator=(const String &other)
    {
        if (this != &other) {
            assign(other.c_str());
        }
        return *this;
    }

    String &operator=(String &&other) noexcept
    {
        if (this != &other) {
            free(_data);
            _data = other._data;
            _length = other._length;
            other._data = nullptr;
            other._length = 0;
        }
        return *this;
    }

    size_t length() const { return _length; }
    bool isEmpty() const { return _length == 0; }
    const char *c_str() const { return _data == nullptr ? "" : _data; }

    int toInt() const { return (int)strtol(c_str(), nullptr, 10); }
    float toFloat() const { return strtof(c_str(), nullptr); }

    bool equals(const String &other) const { return strcmp(c_str(), other.c_str()) == 0; }

    String &operator+=(const String &other)
    {
        size_t rhs_len = other.length();
        char *new_buf = (char *)realloc(_data, _length + rhs_len + 1);
        if (new_buf == nullptr) {
            return *this;
        }

        memcpy(new_buf + _length, other.c_str(), rhs_len + 1);
        _data = new_buf;
        _length += rhs_len;
        return *this;
    }

    String operator+(const String &other) const
    {
        String out(*this);
        out += other;
        return out;
    }

    bool operator==(const String &other) const { return equals(other); }
    bool operator!=(const String &other) const { return !equals(other); }

    char operator[](size_t index) const { return (index < _length) ? _data[index] : '\0'; }
    char &operator[](size_t index)
    {
        static char dummy = '\0';
        if (index >= _length) {
            return dummy;
        }
        return _data[index];
    }

    operator const char *() const { return c_str(); }

private:
    void assign(const char *value)
    {
        size_t len = strlen(value);
        char *new_buf = (char *)malloc(len + 1);
        if (new_buf == nullptr) {
            return;
        }

        memcpy(new_buf, value, len + 1);
        free(_data);
        _data = new_buf;
        _length = len;
    }

    char *_data = nullptr;
    size_t _length = 0;
};

#endif
