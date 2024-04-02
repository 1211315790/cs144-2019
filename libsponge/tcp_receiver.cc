#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment& seg) {
    /*
    ｜             [SYN]  data/payload   [FIN]    ｜
    ｜      seqno    10    11   12   13    14     ｜
    ｜absolute seqno  0    [1    2]   3     4     ｜
    ｜                                ⬆          ｜
    ｜                                ⬆          ｜
    ｜                                ⬆          ｜
    ｜                            abs_seq==3      ｜
    ｜ stream index        [0    1]   2           ｜
    ｜                                ⬆          ｜
    ｜                                ⬆          ｜
    ｜                                ⬆          ｜
    ｜                          next_index==2     ｜
    */
    bool fin_flag = _reassembler.stream_out().input_ended();
    //等待第一个SYN
    if (_syn_flag == false && seg.header().syn == false) return false;
    //因网络延迟而导致的重复SYN,丢弃
    if (_syn_flag == true && seg.header().syn == true) return false;
    //因网络延迟而导致的重复FIN,丢弃
    if (fin_flag == true && seg.header().fin == true) return false;
    //SYN
    if (seg.header().syn)
    {
        _syn_flag = true;
        _isn = seg.header().seqno;
        //FIN
        if (seg.header().fin) {
            _reassembler.stream_out().end_input();
        }
        return true;
    }
    /*判断接收的数据能否放入"接收窗口"*/
    size_t next_index = _reassembler.stream_out().bytes_written() + 1;//checkpoint
    size_t abs_seqno = unwrap(seg.header().seqno, _isn, next_index);
    std::string data = seg.payload().copy();
    uint64_t left, right;//[left,right):
    left = _reassembler.stream_out().bytes_written();
    right = left + window_size();
    size_t len = data.size() + (seg.header().fin ? 1 : 0);
    if (!(abs_seqno - 1 >= right || abs_seqno - 1 + len <= left)) {
        _reassembler.push_substring(data, abs_seqno - 1, seg.header().fin);
        return true;
    }
    else
        return false;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
     /*
    ｜             [SYN]  data/payload   [FIN]    ｜
    ｜      seqno    10    11   12   13    14     ｜
    ｜absolute seqno  0    [1    2]   3     4     ｜
    ｜                                ⬆          ｜
    ｜                                ⬆          ｜
    ｜                                ⬆          ｜
    ｜                            abs_seq==3      ｜
    ｜ stream index        [0    1]   2           ｜
    ｜                                ⬆          ｜
    ｜                                ⬆          ｜
    ｜                                ⬆          ｜
    ｜                          next_index==2     ｜
    */
    if (!_syn_flag) return std::nullopt;
    uint64_t next_index = _reassembler.stream_out().bytes_written();
    //next_index + 1 == absolute seqno
    uint64_t abs_seq = next_index + 1;

    bool fin_flag = _reassembler.stream_out().input_ended();
    if (fin_flag) {
        return wrap(abs_seq + 1, _isn);
    }
    else {
        return wrap(abs_seq, _isn);
    }
}

size_t TCPReceiver::window_size() const { return  _capacity - _reassembler.stream_out().buffer_size(); }
