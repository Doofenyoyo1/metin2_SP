"""LZO1X in pure Python - the compression inside the client's EterPack
entries (MCOZ) and inside gamedata/item_proto (MIPX).

The panel image has no C compiler and python-lzo has no wheels, so the
client data build carries its own codec. When the `lzo` module (python-lzo)
happens to be importable - the m2pack-lzo image on vps1 - it is used
instead, as it is ~50x faster; both write streams the client's
lzo1x_decompress reads.

compress() is lzo1x_1_compress (minilzo 2.x, D_BITS 14) step for step:
greedy, one hash probe per position, matches M2/M3/M4, the final literal
run and the M4 end marker. decompress() is lzo1x_decompress_safe and takes
any LZO1X stream (the client's packs were made by the official compressor,
which also emits M1 matches).
"""

try:  # pragma: no cover - depends on the image
    import lzo as _lzo
except ImportError:  # pragma: no cover
    _lzo = None

M2_MAX_LEN = 8
M3_MAX_LEN = 33
M4_MAX_LEN = 9
M2_MAX_OFFSET = 0x0800
M3_MAX_OFFSET = 0x4000
M3_MARKER = 32
M4_MARKER = 16
D_BITS = 14
D_MASK = (1 << D_BITS) - 1


class LzoError(ValueError):
    pass


def _store_run(out, src, start, t, first):
    """Literal run of t bytes src[start:start+t] (t > 0) as do_compress
    writes it: folded into the previous match's low bits (t <= 3), one
    byte (t <= 18), or 0 + 255-steps + rest."""
    if first and t <= 238:
        out.append(17 + t)
    elif t <= 3:
        out[-2] |= t
    elif t <= 18:
        out.append(t - 3)
    else:
        tt = t - 18
        out.append(0)
        while tt > 255:
            tt -= 255
            out.append(0)
        out.append(tt)
    out += src[start:start + t]


def _compress_py(src):
    src = bytes(src)
    n = len(src)
    out = bytearray()
    if n <= 20:
        t = n
        ii = 0
    else:
        dic = [0] * (1 << D_BITS)
        ip_end = n - 20
        ii = 0
        ip = 4
        frombytes = int.from_bytes
        while True:
            # literal: skip faster through data that does not repeat
            if ip >= ip_end:
                break
            dv = frombytes(src[ip:ip + 4], 'little')
            dindex = ((0x1824429d * dv) & 0xffffffff) >> (32 - D_BITS) & D_MASK
            m_pos = dic[dindex]
            dic[dindex] = ip
            if m_pos >= ip or src[m_pos:m_pos + 4] != src[ip:ip + 4] or ip - m_pos > 0xbfff:
                ip += 1 + ((ip - ii) >> 5)
                continue
            t = ip - ii
            if t:
                _store_run(out, src, ii, t, not out)
            m_len = 4
            limit = n - ip
            while m_len < limit and src[ip + m_len] == src[m_pos + m_len]:
                m_len += 1
            m_off = ip - m_pos
            ip += m_len
            ii = ip
            if m_len <= M2_MAX_LEN and m_off <= M2_MAX_OFFSET:
                m_off -= 1
                out.append(((m_len - 1) << 5) | ((m_off & 7) << 2))
                out.append(m_off >> 3)
            elif m_off <= M3_MAX_OFFSET:
                m_off -= 1
                if m_len <= M3_MAX_LEN:
                    out.append(M3_MARKER | (m_len - 2))
                else:
                    m_len -= M3_MAX_LEN
                    out.append(M3_MARKER)
                    while m_len > 255:
                        m_len -= 255
                        out.append(0)
                    out.append(m_len)
                out.append((m_off << 2) & 0xff)
                out.append(m_off >> 6)
            else:
                m_off -= 0x4000
                if m_len <= M4_MAX_LEN:
                    out.append(M4_MARKER | ((m_off >> 11) & 8) | (m_len - 2))
                else:
                    m_len -= M4_MAX_LEN
                    out.append(M4_MARKER | ((m_off >> 11) & 8))
                    while m_len > 255:
                        m_len -= 255
                        out.append(0)
                    out.append(m_len)
                out.append((m_off << 2) & 0xff)
                out.append((m_off >> 6) & 0xff)
        t = n - ii
    if t > 0:
        _store_run(out, src, n - t, t, not out)
    out += b'\x11\x00\x00'
    return bytes(out)


def _copy_match(out, m_pos, length):
    if m_pos < 0:
        raise LzoError('lookbehind overrun')
    dist = len(out) - m_pos
    if dist >= length:
        out += out[m_pos:m_pos + length]
    else:
        # overlapping: the pattern of `dist` bytes repeats
        chunk = out[m_pos:]
        reps, rest = divmod(length, dist)
        out += chunk * reps + chunk[:rest]


def _decompress_py(src, out_len=None):
    src = bytes(src)
    n = len(src)
    out = bytearray()
    ip = 0

    def need(k):
        if ip + k > n:
            raise LzoError('input overrun')

    state = 'start'
    t = 0
    if n < 3:
        raise LzoError('input too short')
    if src[0] > 17:
        t = src[0] - 17
        ip = 1
        need(t)
        out += src[ip:ip + t]
        ip += t
        state = 'match_next_read' if t < 4 else 'first_literal_run'
    else:
        state = 'loop'
    while True:
        if state == 'loop':
            need(1)
            t = src[ip]
            ip += 1
            if t >= 16:
                state = 'match'
                continue
            if t == 0:
                while True:
                    need(1)
                    if src[ip]:
                        break
                    t += 255
                    ip += 1
                t += 15 + src[ip]
                ip += 1
            need(t + 3)
            out += src[ip:ip + t + 3]
            ip += t + 3
            state = 'first_literal_run'
            continue
        if state == 'first_literal_run':
            need(1)
            t = src[ip]
            ip += 1
            if t >= 16:
                state = 'match'
                continue
            need(1)
            m_pos = len(out) - (1 + M2_MAX_OFFSET) - (t >> 2) - (src[ip] << 2)
            ip += 1
            _copy_match(out, m_pos, 3)
            state = 'match_done'
            continue
        if state == 'match_next_read':
            need(1)
            t = src[ip]
            ip += 1
            state = 'match'
            continue
        if state == 'match':
            if t >= 64:
                need(1)
                m_pos = len(out) - 1 - ((t >> 2) & 7) - (src[ip] << 3)
                ip += 1
                _copy_match(out, m_pos, (t >> 5) - 1 + 2)
            elif t >= 32:
                t &= 31
                if t == 0:
                    while True:
                        need(1)
                        if src[ip]:
                            break
                        t += 255
                        ip += 1
                    t += 31 + src[ip]
                    ip += 1
                need(2)
                m_pos = len(out) - 1 - ((src[ip] | (src[ip + 1] << 8)) >> 2)
                ip += 2
                _copy_match(out, m_pos, t + 2)
            elif t >= 16:
                m_pos = len(out) - ((t & 8) << 11)
                t &= 7
                if t == 0:
                    while True:
                        need(1)
                        if src[ip]:
                            break
                        t += 255
                        ip += 1
                    t += 7 + src[ip]
                    ip += 1
                need(2)
                m_pos -= (src[ip] | (src[ip + 1] << 8)) >> 2
                ip += 2
                if m_pos == len(out):
                    # end of stream
                    if out_len is not None and len(out) != out_len:
                        raise LzoError('output length %d, expected %d' % (len(out), out_len))
                    return bytes(out)
                m_pos -= 0x4000
                _copy_match(out, m_pos, t + 2)
            else:
                need(1)
                m_pos = len(out) - 1 - (t >> 2) - (src[ip] << 2)
                ip += 1
                _copy_match(out, m_pos, 2)
            state = 'match_done'
            continue
        if state == 'match_done':
            t = src[ip - 2] & 3
            if t == 0:
                state = 'loop'
                continue
            need(t)
            out += src[ip:ip + t]
            ip += t
            state = 'match_next_read'
            continue


def compress(data):
    """LZO1X-1 stream of data (no header), the client's format."""
    if _lzo is not None:
        return _lzo.compress(bytes(data), 1, False)
    return _compress_py(data)


def decompress(data, out_len):
    """data (an LZO1X stream) decompressed; out_len is the expected size."""
    if _lzo is not None:
        return _lzo.decompress(bytes(data), False, out_len)
    return _decompress_py(data, out_len)
