#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{ random_device()() }))
    , _initial_retransmission_timeout{ retx_timeout }
    , _stream(capacity) {
    _retransmission_timeout = _initial_retransmission_timeout;
}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    TCPSegment seg;
    //First SYN did not transmit payload.
    if (!_syn_flag) {
        seg.header().syn = true;
        send_segment(seg);
        _syn_flag = true;
        return;
    }
    //零窗口探测
    if (_window_size == 0) {
        send_empty_segment();
        return;
    }
     // when window isn't full and never sent FIN
    while (!_fin_flag) {
        assert(_window_size >= (_next_seqno - _recv_ackno));
        size_t remain = (_window_size - (_next_seqno - _recv_ackno));  // window's free space
        if (remain == 0)
            break;
        remain = min(remain, _stream.buffer_size());
        size_t size = min(TCPConfig::MAX_PAYLOAD_SIZE, remain);
        seg.payload() = Buffer(_stream.read(size));
         // 流关闭了且不存在数据
        if (_stream.input_ended() && _stream.buffer_size() == 0) {
            seg.header().fin = true;
            _fin_flag = true;
        }
        // stream is empty
        if (seg.length_in_sequence_space() == 0) {
            return;
        }
        send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param _window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    /*
            +++接收方已经收到++ ++已经发送但未收到++  ++++还未发送++++
            ++             ++ ++             ++  ++           ++
            ++             ++ ++             ++  ++           ++
            ++_____________++_++_____________++__++___________++
            | 0 | 1 | 2 | 3 | 4 | 5| 6 | 7 | 8 | 9 | 10 |xxxxxx   absolute seqno
            ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
                              ⬆                 ⬆
                              ⬆                 ⬆
                              ⬆                 ⬆
                          recv_ackno         next_seqno
        [0,recv_ackno):接收方已经收到的seqno
        [recv_ackno,next_seqno):已经发送但未收到的seqno
        (next_seqno-recv_ackno):当前占用的窗口数量
        window_size-(next_seqno-recv_ackno):接收方的剩余窗口数量
    */
    size_t abs_ackno = unwrap(ackno, _isn, _recv_ackno);
    // out of window, invalid ackno
    if (abs_ackno > _next_seqno) {
        return false;
    }
    _window_size = window_size;
    // ack has been received
    if (abs_ackno <= _recv_ackno) {
        return true;
    }
    _recv_ackno = abs_ackno;
    //delete outstanding segment
    while (_segments_outstanding.size()) {
        TCPSegment seg = _segments_outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= abs_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
        }
        else {
            break;
        }
    }
    _retransmission_timeout = _initial_retransmission_timeout;
    // if have other outstanding segment, restart timer
    if (!_segments_outstanding.empty()) {
        _timer_running = true;
        _timer = 0;
    }
    _consecutive_retransmissions_count = 0;
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    if (_timer >= _retransmission_timeout && !_segments_outstanding.empty()) {
        _segments_out.push(_segments_outstanding.front());
        _consecutive_retransmissions_count++;
        _retransmission_timeout *= 2;
        _timer_running = true;
        _timer = 0;
    }

}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::send_segment(TCPSegment& seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_outstanding.push(seg);
    _segments_out.push(seg);
    //restart timer
    if (_timer_running == false) {
        _timer_running = true;
        _timer = 0;
    }
}