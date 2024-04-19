#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    _capacity = capacity;
}

size_t ByteStream::write(const string& data) {
    size_t write_count = 0;
    size_t remain_cap = remaining_capacity();
    if (data.size() > remain_cap) {
        write_count = remain_cap;
    }
    else {
        write_count = data.size();
    }
    _write_count += write_count;
    _buffer_size += write_count;
    _deq.push_back(std::string(data.begin(), data.begin() + write_count));
    return write_count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = len;
    std::string ret;
    for (size_t i = 0;i < _deq.size() && length>0;i++) {
        if (length < _deq[i].size()) {
            ret = ret + _deq[i].substr(0, length);
            length = 0;
        }
        else {
            ret = ret + _deq[i];
            length -= _deq[i].size();
        }
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    _buffer_size -= len;
    size_t length = len;
    while (length > 0) {
        if (length < _deq.front().size()) {
            _deq.front() = std::move(_deq.front().substr(length));
            _read_count += length;
            length = 0;
        }
        else {
            length -= _deq.front().size();
            _read_count += _deq.front().size();
            _deq.pop_front();
        }
    }

}

void ByteStream::end_input() { _input_ended_flag = true; }

bool ByteStream::input_ended() const { return _input_ended_flag; }

size_t ByteStream::buffer_size() const {
    return _buffer_size;
}

bool ByteStream::buffer_empty() const {
    return buffer_size() == 0;
}

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
