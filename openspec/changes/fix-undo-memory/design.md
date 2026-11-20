## Context

The installed history check uses `INTERNAL | 8BIT`, but the historical OTA figures behind its reserve count all `INTERNAL` memory. The heap samples are not interchangeable. A score write can also be acknowledged by Arduino before the callback discovers a full queue.

## Goals / Non-Goals

Preserve thousands of reversible gameplay actions and all scoring/panel behavior. Keep OTA's existing resource gates and Android unchanged. Do not persist scores or history across reboot or change the maintenance transport.

## Decisions

- Reserve 6,144 static bytes for the existing inverse encoding. Use one byte per ordinary transition and three bytes for typical Resets; keep the existing full-width representation with a tag for large scores. Refuse atomically at byte exhaustion without eviction. Keep the owner in the loop, across connect/disconnect. No runtime growth, allocator metadata or heap fragmentation. This avoids either tuning a shared reserve by guesswork or wearing flash for volatile state.
- Keep the static budget small enough to leave margin against archived OTA samples; audit the actual linked RAM change. OTA still checks current internal free and largest internal byte blocks at each stage, not projected memory or the Undo budget.
- Interpose only the Arduino GATTS callback registration, handle command writes by their assigned handle, and forward other events to Arduino unchanged. A fixed FreeRTOS queue stays the sole transport-to-loop handoff. Reserve a queue slot before sending ATT success; send ATT insufficient resources for a full queue. Do not block the BLE task or use unacknowledged writes. The existing extended state refusal still handles journal capacity in the loop.

## Risks / Trade-offs

Static memory is paid even when the journal is empty. Extremely long sessions eventually fill it; Undo releases journal space, not general-purpose heap. Bench checks must measure the new candidate during scoring and OTA; historical samples do not certify RF or installation. The callback bridge depends on the pinned Arduino event plumbing; verify the linked route in each production profile before signing.
