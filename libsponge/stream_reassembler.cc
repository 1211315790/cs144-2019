#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const std::string& data, const size_t index, const bool eof) {
    //防止还有未装配完毕的字符
    if (eof) {
        _eof_index = index + data.size();
        _eof_flag = true;
    }
    //暂存
    if (_mp.count(index) == 0) {
        _mp[index] = data;
    }
    else if (_mp[index].size() < data.size()) {
        _mp[index] = data;
    }
    //看看是否可以合并
    string s;
    size_t tmp_next_offset = _next_offset;
    std::vector<uint64_t> delete_key;
    for (auto [key, value] : _mp) {
        if (key <= tmp_next_offset && key + value.size() >= 1 && key + value.size() - 1 >= tmp_next_offset)
        {
            s += value.substr(tmp_next_offset - key);
            tmp_next_offset += value.size() - (tmp_next_offset - key);
            delete_key.push_back(key);
        }
    }
    //将合并后的字串删除
    for (auto key : delete_key) {
        _mp.erase(key);
    }
    if (s.size() > 0) {
        size_t len = s.size();
        size_t remain_assemble = _output.bytes_written() - _output.bytes_read();
        size_t remain_space = _capacity - remain_assemble;
        if (len <= remain_space) {  //判断剩余的capacity是否够完整写入s
            _output.write(s);
            _next_offset += s.size();
        }
        else {
            _output.write(s.substr(0, remain_space));
            _next_offset += remain_space;
        }
    }
    //出现eof且装配完毕
    if (_eof_flag && (_next_offset == _eof_index)) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    std::set<size_t> remain_assemble_char_index;
    for (auto& [index, data] : _mp) {
        for (size_t i = 0; i < data.size(); ++i) {
            if (index + i >= _next_offset) remain_assemble_char_index.insert(index + i);
        }
    }
    return remain_assemble_char_index.size();
}

bool StreamReassembler::empty() const { return _unassembled_byte == 0; }
