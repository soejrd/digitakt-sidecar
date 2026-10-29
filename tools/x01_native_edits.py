"""Native UI edit scenarios for the booted X01 emulator, never hardware."""
import struct

import i03_dynamic_probe as dynamic


def copy_src(session, step, peer, symbols):
    """Copy a native SRC page across two already-negotiated USB1 tracks."""
    uc = session.m.uc
    kit = struct.unpack('>I', uc.mem_read(int(symbols['fw_kit'], 0), 4))[0]
    generation = peer.peer_session()

    def sounds():
        return [dynamic._sound_slots(uc, kit, t) for t in range(8)]

    def params():
        return [[peer.peer_param(t, k) for k in range(8)] for t in range(2)]

    def gesture(key):
        # Native SRC's vtable routes held SRC + transport to page actions.
        # See elekloader's digitakt_src_storage native dispatcher regression.
        session.press(session.code('SRC')); step(60)
        session.press(session.code(key)); step(40)
        session.release(session.code(key)); step(60)
        session.release(session.code('SRC')); step(150)

    def trigger(track):
        session.press(24 + track); step(30)
        session.release(24 + track); step(150)

    def select(track):
        session.press(session.code('TRK')); step(50)
        session.press(24+track); step(30)
        session.release(24+track); step(100)
        session.release(session.code('TRK')); step(100)
        actual=struct.unpack('>I',uc.mem_read(int(symbols['fw_active_track'],0),4))[0]
        assert actual==track,actual

    select(1)
    session.press(session.code('SRC')); step(30)
    session.release(session.code('SRC')); step(100)
    for encoder in range(1,9):
        for _ in range(8):
            session.turn(encoder,4,after_ms=0); step(25)
    trigger(1)
    # Panel edits are natively smoothed. Compare settled final engine values
    # against the integer values the transport promises, not an intermediate.
    expected=[max(0,min(127,v//256))*256 for v in sounds()[1][17:25]]
    for _ in range(100):
        if params()[1]==expected: break
        step(5)
    assert params()[1]==expected, (params()[1],expected)
    before_sound, before_host = sounds(), params()
    assert all(a!=b for a,b in zip(before_host[0],before_host[1])),before_host
    gesture('RECORD')
    select(0)
    gesture('STOP')
    trigger(0)
    for _ in range(100):
        if params()[0]==before_host[1]: break
        step(5)
    after_sound, after_host = sounds(), params()
    checks = {
        'all_eight_controls_distinct_before': all(a!=b for a,b in zip(before_host[0],before_host[1])),
        'target_src_matches_source': after_sound[0][17:25] == before_sound[1][17:25],
        'target_host_matches_source': after_host[0] == before_host[1],
        'source_host_unchanged': after_host[1] == before_host[1],
        'source_sound_unchanged': after_sound[1] == before_sound[1],
        'other_six_sounds_unchanged': after_sound[2:] == before_sound[2:],
        'target_other_slots_unchanged': all(after_sound[0][k] == before_sound[0][k]
                                            for k in range(53) if not 17 <= k < 25),
        'same_active_session': peer.peer_session() == generation and bool(peer.peer_allowed()),
        'no_host_fault': peer.peer_fault() == 0,
    }
    return {
        'scope': 'booted OS1.54 native held-SRC copy/paste across two USB1 tracks; real host engines; modelled USB; no hardware',
        'gesture': 'track 2: hold SRC + RECORD; track 1: hold SRC + STOP; trigger target',
        'host_params_before_8_8': before_host,
        'host_params_after_8_8': after_host,
        'src_slots_before_8_8': [v[17:25] for v in before_sound[:2]],
        'src_slots_after_8_8': [v[17:25] for v in after_sound[:2]],
        'checks': checks, 'pass': all(checks.values()),
        'limits': ['SRC page copy only, not track/pattern copy or saved-project persistence.'],
    }


def sound_lock(session, step, peer, symbols, native_starts):
    """Seed two disposable sound-pool entries and let native playback resolve them."""
    uc = session.m.uc
    kit = struct.unpack('>I', uc.mem_read(int(symbols['fw_kit'], 0), 4))[0]
    pattern = struct.unpack('>I', uc.mem_read(dynamic.PATTERN_POINTER, 4))[0]
    assert 0x40000000 <= pattern < 0x48000000
    generation = peer.peer_session()
    base = [dynamic._sound_slots(uc, kit, t) for t in range(8)]
    # OS1.54's native fire stores the signed sound-lock byte at track+0x240
    # in its event, then resolves nonnegative entries as pool+162*index.
    # The pool's runtime base is the immediate at native fire 0x4006f7e0.
    pool = 0x41977466
    for t in range(2):
        sound = bytearray(uc.mem_read(kit + 32 + 162*t, 162))
        assert sound[126] == 121
        struct.pack_into('>h', sound, 20 + 2*17, (72+12*t)<<8)
        uc.mem_write(pool + 162*t, bytes(sound))
        track = pattern + dynamic.TRACK_STRIDE*t
        uc.mem_write(track, struct.pack('>64H', *([0x0881]*4+[0]*60)))
        uc.mem_write(track+0x240, b'\xff'*64)
        uc.mem_write(track+0x242, bytes([t]))
        locks = pattern+0x38f0+6965*t
        for index in range(4):
            dynamic._write_i16s(uc, locks+108*index, [-1]*53+[0])
    uc.mem_write(pattern+0x1ec5b, b'\x02')
    counts = [peer.peer_starts(t) for t in range(2)]
    seen = [[], []]
    session.press(session.code('PLAY')); step(40); session.release(session.code('PLAY'))
    for _ in range(600):
        step(5)
        for t in range(2):
            count = peer.peer_starts(t)
            if count != counts[t]:
                seen[t].append(peer.peer_param(t, 0)); counts[t] = count
    session.press(session.code('STOP')); step(40)
    session.release(session.code('STOP')); step(100)
    expected = [[60<<8, (72+12*t)<<8, 60<<8] for t in range(2)]
    checks = {
        'native_lock_then_base_restore': all(any(v[i:i+3] == expected[t]
            for i in range(len(v)-2)) for t, v in enumerate(native_starts)),
        'host_starts_match_native': seen == native_starts,
        'base_sounds_preserved': base == [dynamic._sound_slots(uc, kit, t) for t in range(8)],
        'same_active_session': generation == peer.peer_session() and bool(peer.peer_allowed()),
        'no_host_fault': peer.peer_fault() == 0,
    }
    return {
        'scope': 'booted OS1.54 seeded native sound-pool locks on two USB1 tracks; real host engines; modelled USB; no hardware',
        'fixture': 'four native trigs per track; sound-pool indices 0/1 only on step 3; no parameter locks',
        'native_start_values_8_8': native_starts, 'host_start_values_8_8': seen,
        'expected_consecutive_starts': expected, 'checks': checks,
        'pass': all(checks.values()),
        'limits': ['Disposable RAM pool/pattern fixture, not UI sound-pool loading or saved-project persistence.'],
    }
