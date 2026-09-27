# Contribution preparation: NanoELS and openHASP

Research snapshot: 2026-09-27. No maintainer was contacted and no upstream PR was
opened. Working branches are in the owner's forks for review.

## NanoELS

Upstream: [kachurovskiy/nanoels](https://github.com/kachurovskiy/nanoels).
The inspected source snapshot has an MIT license; no CONTRIBUTING file or PR
submission template was found in that snapshot. This is not a guarantee that
maintainers have no additional expectations. Recheck the default branch and
repository discussions/issues before proposing the feature.

Prepare the H5 backend adapter as the first focused change: preserve Nextion as
default, explain runtime selection/reboot semantics and the panel dependency,
include host tests, full H5 build evidence and measured size delta. State missing
physical testing, openHASP audio and link-loss watchdog plainly. Keep mode icon
assets/layout documentation in a separate layout-focused change where practical.

Suggested proposal: “Optional openHASP display backend for H5, preserving the
existing Nextion interface and machine workflow.” Ask whether the fixed mapping
and runtime backend preference fit the maintainer's intended configuration model.
Do not combine a future general template manager into the initial adapter PR.

## openHASP

Upstream: [HASwitchPlate/openHASP](https://github.com/HASwitchPlate/openHASP).
Read the current [CONTRIBUTING.md](https://github.com/HASwitchPlate/openHASP/blob/7bc826e8cf4dbfbaa1e4a702f2de02684ef41a7f/CONTRIBUTING.md)
and [PR template](https://github.com/HASwitchPlate/openHASP/blob/7bc826e8cf4dbfbaa1e4a702f2de02684ef41a7f/.github/PULL_REQUEST_TEMPLATE.md).
They request small, single-goal PRs and separation of new-device work from core
changes. The template asks for real hardware tests where applicable, a clean
build for at least one existing board, and updated documentation. Those panel
build/hardware gates are not yet satisfied.

1. **Generic UART extension:** opt-in serial command/event transport, portable
   byte HAL, bounded framing, MQTT coexistence and layout-ready ordering. Explain
   why the one layout-loaded hook is needed. Ask whether this belongs in custom
   extension support or a maintained core transport before polishing a PR.
2. **DIS02050A board support:** independent profile plus STC initialization and
   backlight hooks. Submit separately after revision identification, full builds
   and real display/touch/brightness tests. Keep UART optional in this proposal.
3. **Nextion compatibility library:** later design proposal; runtime subset and
   limits first. It is not included in either change above.

Our panel branch is deliberately based on release 0.7.0-rc13 at the owner's
request. An upstream PR should use a fresh branch based on current upstream
master and port the focused changes, resolving newer conventions there. Preserve
the release-based delivery branch; do not silently change its base. Although the
working board branch includes the UART branch as an ancestor, extract the board
patch alone for a board-only upstream PR.

## Pre-submission checklist

- [ ] Owner approves scope and public outreach.
- [ ] Refresh upstream branches, policies, existing issues and overlapping work.
- [ ] Rebase/port each focused patch to the requested target branch.
- [ ] Preserve license notices; document original icon provenance/font license.
- [ ] Complete full panel and existing-board builds; attach exact versions/logs.
- [ ] Complete hardware tests and retain serial captures/photos.
- [ ] Explain behavior, dependencies, limitations and test results in PR body.
- [ ] Leave physical-test boxes unchecked until actually completed.

No blanket CLA/DCO conclusion is made; check repository rules at submission.
