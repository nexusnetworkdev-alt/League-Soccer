# Soccerverse 3D Replay PoC

## Goal

Validate a single visual bridge:

`Soccerverse event chain -> deterministic 3D staging -> League-Soccer player models / ball / stadium / replay camera`

This is intentionally not NEXUS integration yet.

## Golden sequence

Fixture: `#387016`

Source interval: `81:34 -> 82:21`

Ordered Soccerverse action:

`Balogun -> Jensen -> Nygren -> Ambros -> Ramirez -> Turay -> Garcia -> Ramirez -> shot -> Krahl save`

## Provenance boundary

The ordered event chain above is source-derived.

League-Soccer does not receive historical Soccerverse tracking coordinates. Therefore the XY positions, pass paths, shot arc, actor movement and 0-8.5 second presentation timing in this PoC are reconstructed deterministically for visualization only.

The product must keep these layers separate:

1. source event / timestamp;
2. derived event interpretation;
3. reconstructed spatial staging;
4. rendered 3D state.

## How the PoC is triggered

Start any League-Soccer match, open the pause menu, and choose:

`Soccerverse #387016 - 3D PoC`

The normal match stays paused. The replay page reuses the current 3D stadium, seven outfield player models, the defending goalkeeper, the ball geometry and League-Soccer replay cameras.

The PoC restores the live visual positions when leaving the replay page.

## Current limitations

- League-Soccer actors are stand-in models, not yet matched to Soccerverse identities.
- Event-specific body animations (pass / receive / shot / save) are not yet forced; the first milestone is validating the actual 3D visual bridge.
- Non-actor players retain their current paused-match positions.
- No source-derived XY tracking is claimed.
- No NEXUS UI or API ingestion is included in this branch.

## Next milestone

If the visual result is acceptable, replace the fixture-specific data with a generic external replay schema and add event-aware animation selection, player identity mapping and formation context.

## CI note

GitHub Actions was enabled on the fork after the initial pull request was opened. This documentation-only commit intentionally retriggers the pull-request workflow so the PoC branch can be compiled and validated on Linux, macOS and Windows before any merge.
