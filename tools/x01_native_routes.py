"""Native UI route changes for X01's disposable booted emulator."""
import json
import struct


def exercise(session, step, peer, symbols, trace, audio):
    uc = session.m.uc
    phases = []

    def word(name):
        return struct.unpack('>I', uc.mem_read(int(symbols[name], 0), 4))[0]

    def tap(code):
        session.press(code); step(30)
        session.release(code); step(100)

    def choose(track, external):
        session.press(session.code('TRK')); step(50)
        tap(24+track)
        session.release(session.code('TRK')); step(100)
        assert word('fw_active_track') == track
        session.press(session.code('FUNC')); step(80)
        tap(session.code('SRC'))
        session.release(session.code('FUNC'))
        # Clamp to the first machine, then choose USB1 (fifth entry) or stock.
        for _ in range(8): tap(14)  # UP
        if external:
            for _ in range(4): tap(15)  # DOWN
        tap(12); tap(12)  # YES, close chooser
        kit = word('fw_kit')
        machine = uc.mem_read(kit+32+162*track+126,1)[0]
        assert (machine == 121) == external, (track,external,machine)
        tap(24+track)  # Commit the sound to native render state.
        step(1200)
        # Do not add another panel event here: recovery must complete without
        # an incidental UI/RX wake after the route has become valid.

    def phase(name, tracks, previous=None):
        selected = [t for t in range(8)
                    if uc.mem_read(int(symbols['fw_render_machine'],0)+t,1)[0] == 121]
        expected = (tracks+[127,127])[:2]
        supported = 0 < len(tracks) <= 2
        row = {'name':name,'selected_tracks':selected,'packed_lanes':word('x01_route_lanes'),
               'route':word('usb_track_route'),'device_gate':word('usb_track_gate'),
               'host_allowed':bool(peer.peer_allowed()),'session':peer.peer_session(),
               'host_fault':peer.peer_fault()}
        row['pass'] = selected == tracks and bool(row['route']) == supported and \
                      bool(row['device_gate']) == supported and row['host_allowed'] == supported
        if supported:
            row['pass'] &= row['packed_lanes'] == expected[0]+256*expected[1]
            if previous is not None: row['pass'] &= row['session'] != previous
            row['pass'] &= all(audio[t] > 0 for t in tracks) and row['host_fault'] == 0
        phases.append(row)
        if not row['pass']:
            raise AssertionError({'phase':row,'phases':phases,'recent_transitions':trace[-40:],
                                  'worker_runs':word('usb_track_worker_runs'),
                                  'ui_ticks':word('usb_track_diag_ui_ticks'),'host_reason':peer.peer_reason()})
        print(json.dumps(row),flush=True)
        return row['session']

    initial = phase('initial pair', [0,1])
    choose(0,False)
    single = phase('remove lower track; remaining track becomes lane 0',[1],initial)
    choose(2,True)
    shifted = phase('add track 3; pair maps tracks 2/3',[1,2],single)
    choose(0,True)
    phase('third external voice unsupported; all selected sources silent',[0,1,2])
    choose(2,False)
    restored = phase('remove third voice; recover original pair',[0,1],shifted)
    choose(0,False)
    phase('single track again',[1],restored)
    choose(1,False)
    phase('no external tracks; output closed',[])
    choose(0,True)
    recovered = phase('recover from no external tracks',[0],restored)
    choose(1,True)
    phase('recover original pair after empty route',[0,1],recovered)
    return {'scope':'booted OS1.54 native machine chooser and actual X01 worker/dual-engine peer; modelled USB; no hardware',
            'phases':phases,'route_transitions':trace,'audible_mapped_blocks_by_track':audio,
            'checks':['nonselected source buffers preserved at every observed render boundary',
                      'every gated selected source equals its negotiated USB PCM lane',
                      'all selected sources zero whenever device gate is closed',
                      'supported route changes negotiate fresh sessions',
                      'zero/three selected tracks close device and host output; supported routes recover'],
            'pass':all(p['pass'] for p in phases),
            'limits':['Machine changes use native UI while manually triggered voices run; sequencer playback and physical timing are not qualified.']}
