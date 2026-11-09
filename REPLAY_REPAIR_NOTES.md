# Replay repair handoff for CO2

Contribution branch: `replay/repair-source-playback`, based on
`feature/replay-port-repair-v5` at `fca30006eef0535e32682b3af3cb3b40889bec4d`.

The actual client package uses the Westwood 3.03 executable plus assembly
patches. The main implementation and runtime validation therefore live in the
separate local `../ra-patches` checkout, on `replay/local-record-playback`.
See `../ra-patches/tools/replay/README.md` for the exact binary hash, hooks,
build instructions, launch-context preservation and runtime test results.
These source changes are a separate repair set, not a replacement package binary.

## Source changes

`CODE/QUEUE.CPP`:

- Skirmish recording uses `Queue_AI_Normal`, which records frame zero and executes
  only the local house. Playback now does both too. Previously it skipped the first
  frame and then treated skirmish like multiplayer for house execution.
- Restore the multiplayer CRC calculation and its 32-frame history. It consumes
  an RNG value and participates in FRAMEINFO checks; it is not unused bookkeeping.
  Playback calls it only for multiplayer, on the same schedule as recording.
  This preserves the original multiplayer RNG cadence instead of disabling live
  out-of-sync detection. Do not add it to campaign/skirmish playback.
- Consume recorded FRAMEINFO without comparing its peer-specific transport CRC
  to the viewer's history. Live heartbeat validation remains enabled.
- A remote EXIT now calls the same `Destroy_Connection` routine as the live match,
  including player removal. The package's assembly implementation additionally
  has spawner-specific army destruction, which the native replay AI conversion
  did not reproduce.
- Bound counts by the real DoList capacity (`MAX_EVENTS * 64`, not `MAX_EVENTS`),
  reject invalid event types/frames/house IDs, check queue insertion, and reject a
  nonpositive send rate before performing division in the network branch.
- Clear outgoing viewer commands. Opening playback options returns immediately
  instead of also adding an OPTIONS event to that unused queue.
- Check both count and event writes. A short write closes/disables recording
  while the live match continues.

`CODE/INIT.CPP`:

- Create `replays` before opening an automatic recording in it.
- Skip UDP setup when `PlayReplay` was requested; the recorded session is loaded
  afterward and playback does not need live transport initialization.

## Validation

The final changes built successfully with the repository's Watcom 10.6 dependency
archive and its unmodified `MAKE.BAT`, for **release, playtest and internal**.
Outputs are in `RUN/ENGLISH/`; the full log is `local-build.log` (local-only).
The DOS assembler requires a short path; a temporary `SUBST R:` was used.

The corresponding Westwood binary repairs have actual-engine evidence:

- 4,863 skirmish frame/RNG/CRC samples matched on playback.
- A guarded-build skirmish with deployment, speed change and exit matched
  all 302 samples.
- Two localhost players recorded deployment, speed changes and exit events. Each
  player's offline playback matched all 320 recorded samples (protocol 2 / rate 3).
  The package defaults (protocol 0 / rate 5) matched all 329 samples for each player.
- 20 emitted-x86 hook tests passed, including the later sidebar/input repairs.
- A longer real client-started recording matched all 11,522 samples, including
  playback through the existing client's Play button and the native adapter.

The source-built executable has **compile validation only** here; those runtime
results belong to the separately patched package executable. The raw replay
formats must not be assumed interchangeable: they serialize build-dependent
class layouts, and the package historically NOPed out the seed field.

## Remaining issues in this source branch

`Save_Recording_Values` writes the session, seed, scenario name, Special and Options.
It does not preserve the complete spawner context: per-slot spawn locations,
countries, handicaps, spectator slots, scenario difficulty, expansion/fast-build
flags and rule overrides. Loading through a later, unrelated SPAWN.INI can thus
construct a different scenario before any recorded command executes. The map
itself can also be replaced by the next client launch.

The assembly implementation's adapter now embeds the native stream and launch
INIs/map in a client-compatible portable container, with installed-input
fingerprints. It reuses the client's recording/list/Play functionality without
client-source changes. That adapter targets the Westwood ABI; it cannot be assumed
to read a stream produced by this C++ build. This source branch still needs
equivalent launch-context preservation, checked header reads, and caller handling
of header-load failure.

The final automated multiplayer tests skip score screens and use sequential
departures; both instances complete without cleanup timeouts. The final diagnostic
playback still returns 1 despite matching all recorded samples, so the client must
handle that status appropriately. Interactive score screens need separate testing.

Campaign selection triggers, local formation state, production/combat/special
weapons, spectators, disconnects and longer real matches need additional coverage.
Save-game starts require embedding the initial save; they cannot be replayed by
reconstructing a fresh scenario from a seed alone. Seeking/checkpoints/statistics
from YR are optional future features, not prerequisites for this minimal route.
