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
    if (data.size() > _capacity - _deq.size()) {
        write_count = _capacity - _deq.size();
    }
    else {
        write_count = data.size();
    }
    for (size_t i = 0;i < write_count;i++) {
        _deq.push_back(data[i]);
        _write_count++;
    }
    return write_count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret;
    int peek_count = len;
    if (len > _deq.size()) {
        peek_count = _deq.size();
    }
    for (int i = 0;i < peek_count;i++) {
        ret += _deq.at(i);
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    int pop_count = len;
    if (len > _deq.size()) {
        pop_count = _deq.size();
        set_error();
    }
    for (int i = 0;i < pop_count;i++) {
        _deq.pop_front();
        _read_count++;
    }
}

void ByteStream::end_input() { _input_ended_flag = true; }

bool ByteStream::input_ended() const { return _input_ended_flag; }

size_t ByteStream::buffer_size() const { return _deq.size(); }

bool ByteStream::buffer_empty() const { return _deq.empty(); }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - _deq.size(); }
