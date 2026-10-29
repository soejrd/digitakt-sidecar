# digiemu patches

Base: `f57c2f237e84b8ff91bc3adb2a2931bccf9928e9` in sources.json.
Apply numbered patches in order with `git apply` from that checkout.

`0001` makes cycle-timed Session counter reads use the current instruction's
position within its precharged basic block. Legacy instruction stepping and
interrupt deadlines retain their existing behavior. DTIM2's external clock is
still unmodeled. No physical memory costs or new clock rates are introduced.

Replay and verify against the working dependency:

```sh
python3 scripts/test_digiemu_replay.py
```

Focused runtime checks (requires the local patched Unicorn runtime):

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python -m unittest discover \
  -s deps/digiemu/tests -p 'test_dtim_counter.py'
```
