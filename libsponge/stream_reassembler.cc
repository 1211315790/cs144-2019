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
    size_t tmp_next_index = _next_index;
    std::vector<uint64_t> delete_index;
    for (auto [key_index, value] : _mp) {
        if (key_index <= tmp_next_index && key_index + value.size() >= tmp_next_index + 1)
        {
            s += std::move(value.substr(tmp_next_index - key_index));
            tmp_next_index += value.size() - (tmp_next_index - key_index);
            delete_index.push_back(key_index);
        }
    }
    //将合并后的字串删除
    for (auto key_index : delete_index) {
        _mp.erase(key_index);
    }
    if (s.size() > 0) {
        size_t len = s.size();
        size_t use_space = _output.bytes_written() - _output.bytes_read();
        size_t remain_space = _capacity - use_space;
        if (len <= remain_space) {  //判断剩余的capacity是否够完整写入s
            _output.write(std::move(s));
            _next_index += s.size();
        }
        else {
            _output.write(std::move(s.substr(0, remain_space)));
            _next_index += remain_space;
        }
    }
    //出现eof且装配完毕
    if (_eof_flag && (_next_index >= _eof_index)) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    std::set<size_t> remain_assemble_char_index;
    for (auto& [index, data] : _mp) {
        for (size_t i = 0; i < data.size(); ++i) {
            if (index + i >= _next_index) remain_assemble_char_index.insert(index + i);
        }
    }
    return remain_assemble_char_index.size();
}

bool StreamReassembler::empty() const { return _unassembled_byte == 0; }
