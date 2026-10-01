# Soccerverse Guided Match Simulation v2

## Objective

Replace the v1 coordinate-staging proof of concept with a live 22-player simulation guided by Soccerverse match data.

The v1 remains the rendering proof. v2 must look like football: all players move, team shapes remain coherent, and League-Soccer's native AI/animation/ball systems stay active.

## Core principle

Soccerverse is the factual director. League-Soccer is the continuous football simulator.

The engine must not invent factual outcomes that contradict Soccerverse during replay mode, but it is allowed to reconstruct the unobserved movement between source anchors.

## Data layers

1. Factual source state
   - fixture / score / match clock
   - historical XI
   - substitutions
   - player roles
   - historical tactical changes
   - structured match events
   - final match commentary

2. Derived interpretation
   - action family: short pass, long pass, through ball, cut-back, dribble, one-on-one, tackle, interception, clearance, shot, chip, save, punch, etc.
   - possession side
   - likely action zone / phase
   - attacking and defending participants

3. Reconstructed simulation state
   - formation geometry
   - team block depth / width
   - support runs
   - defensive shifts and cover
   - ball and player paths between factual anchors

4. Rendered 3D state
   - native League-Soccer PlayerCommand actions
   - native humanoid animations
   - native ball physics
   - native camera / stadium / replay presentation

## Constraint hierarchy

### Hard constraints
- score and goals
- substitutions and injuries
- cards
- tactical / mentality changes
- event order and principal actors

### Strong constraints
- action type from commentary and structured events
- intended passer / receiver / shooter / tackler / goalkeeper
- outcome family: goal, save, miss, post, interception, clearance, etc.

### Soft constraints
- plausible zone
- spacing
- line heights
- defensive pressure
- support movement
- recovery runs

### Free simulation
- micro-movements
- body orientation
- acceleration / deceleration
- off-ball adjustments
- harmless connective possession between factual anchors

## First vertical slice

Use one validated Soccerverse action as the v2 golden test.

Acceptance criteria:
- all 22 players remain active;
- both teams respect their formation / role slots;
- non-participants react to ball movement;
- attacking support and defensive shifting are visible;
- native run/pass/shot/save animations are used where possible;
- no direct per-frame teleporting of the seven attacking actors;
- source event order and final outcome remain consistent with Soccerverse;
- the clip can be replayed deterministically for debugging.

## Commentary parser targets

Initial phrase families include:
- precise / smart pass
- glorious long pass / long ball
- slides the ball across
- cuts the ball back
- finds player in the box
- heel pass
- heads the ball down
- cuts through the defence
- sidesteps / beats marker
- defensive mistake
- one-on-one
- tackle / sliding challenge
- interception / blocked
- clearance
- strike / powerful / vicious shot
- chip
- save / difficult save / comfortable gather / punch
- post / wide / over bar / goal

## Architecture

`Soccerverse capture -> MatchState compiler -> Guided Director -> League-Soccer AI + PlayerCommand -> 3D replay`

The Guided Director should intervene only around factual anchors. Between anchors, League-Soccer should run normally with team shape and AI active.

## Tactical Lab follow-up

Replay mode keeps future Soccerverse anchors hard-constrained.

Tactical Lab mode forks from a historical snapshot and releases future anchors. Alternative tactical choices are then evaluated by repeated simulations with different seeds. Results are descriptive scenario outputs, not claims of causality.

## Milestones

1. 22-player live golden clip with formation discipline.
2. Commentary-to-action-template parser.
3. Generic anchor scheduler for one half.
4. Full-match guided simulation.
5. Historical tactical timeline and substitution fidelity.
6. Tactical Lab scenario branching and multi-run comparison.
