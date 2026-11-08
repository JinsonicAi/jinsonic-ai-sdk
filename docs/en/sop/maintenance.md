# Backup, Migration, Maintenance, and Handover

[简体中文](../../zh/sop/maintenance.md) · [Manual home](index.md)

For process owners, installers, and administrators. A reusable deployment needs traceable changes and recoverable faults, beyond a demonstration in one browser.

## 1. Keep a station record

| Item | Record |
|---|---|
| Station | Name, location, camera view |
| Task | Distinguishable name and device |
| Plan | Name, saved version, stage structure |
| Media | Teaching source, independent footage, demo ranges |
| Targets | Names, reference purposes, confusable items |
| Regions | Functional purpose and physical boundaries |
| Completion | Automatic/manual, hands, timing, order |
| Validation | Expected/actual behavior, issues, scope |
| Responsibility | Approval, configuration, operation, maintenance |
| Backup | Bundle, recordings, date, matching version |

A name alone does not identify applicable evidence. Check version, footage, and viewpoint.

## 2. Before saving

Verify completed drawings, understandable instructions, adoption state, actual region bindings, handedness, combinations, timing, ranges, and change impact. Distinguish passed, failed, untested, and unknown.

Export success, closing a window, or scan completion is not task-save acknowledgement.

## 3. Save and reopen

[![Outer task Save location](../../assets/sop/en-23-save-task.jpg)](../../assets/sop/en-23-save-task.jpg)

1. Select Done in the SOP editor.
2. Confirm the node if its dialog remains.
3. Save in the outer task editor.
4. Wait for acknowledgement; retain the window on failure.
5. Reopen the same task.
6. Inspect every changed setting, not just its name.
7. Inspect shared and dependent steps.
8. Revalidate before operation where required.

### Reopen checklist

- [ ] Target names and reference counts.
- [ ] Pending adoption handled as intended.
- [ ] Region geometry and every binding.
- [ ] Hand capability and direction.
- [ ] Conditions, combination, completion.
- [ ] Units, deadlines, stage order.
- [ ] Independent validation/demo ranges.
- [ ] Readable instructions and playable demos.
- [ ] Version and validation status.

## 4. Export a plan

### 4.1 Procedure

1. Open the intended plan.
2. Verify that the current draft is the intended backup.
3. Select Export plan.
4. Wait for download and retain the original file.
5. Record task, plan, date, and matching version.
6. Do not manually edit package internals to bypass validation.

### 4.2 Contents

The current **.sopbundle** format uses a checked manifest. Depending on the plan, it contains its definition, portable plugin graph, bindings, detector reference images, and assets referenced by instructions/regions.

It is not a whole-device backup. Camera addresses, credentials, runtime state, scan/validation history, and unrelated unreferenced media must not be assumed preserved by that bundle.

### 4.3 Limits

Current bundle material is limited to 64 MiB total, 32 MiB for an individual plan asset, and 8 MiB for a reference image, with additional item-count limits. These are bundle transfer limits, not universal limits for every recording/upload entry.

Resolve explicit size messages by reducing unnecessary content, shortening demonstrations, or dividing stations. Do not remove referenced material blindly. Retain important original recordings separately.

## 5. Import at another station

### 5.1 Prerequisites

Back up the destination plan. Check required object, hand, spatial, and sequence capabilities. Prepare local camera/network credentials and arrange a controlled validation period.

### 5.2 Procedure

1. Open the destination SOP editor.
2. Import the original supported bundle/ZIP.
3. Wait for integrity and capability checks.
4. Read replacement scope. A full import can replace draft steps and plugin combinations; inspect local-camera preservation.
5. Confirm explicitly and wait for assets.
6. Bind local sources in Detection settings when required.
7. Inspect all references, regions, handedness, and rules.
8. Validate at the destination.
9. Save and reopen.
10. Test operator interaction under controlled conditions.

### 5.3 Different models or capabilities

If explicit local adaptation is offered, read what remains and which numerical thresholds need reevaluation. Old validation and recommendations do not establish the adapted plan's performance.

When capability is unavailable, unknown, or incompatible and no valid adaptation is offered, retain the file for an installer rather than bypassing the check.

## 6. Revalidation triggers

| Change | Revalidate at least |
|---|---|
| Camera movement, mirroring, rotation | Regions, direction, hands, dependencies |
| Object appearance or packaging | Target positives/negatives and all uses |
| Similar new tool | Confusion, duplicates, count |
| Gloves, posture, operator position | Hand visibility, handedness, coverage |
| References or gates | Misses, false detections, duplicates, independent footage |
| Shared region | All referencing steps |
| Hold/gap/deadline | Speed variation, pauses, mistakes, interruptions |
| Group/order | Both permitted orders, omissions, reversed dependencies |
| Software/model/device update | Capability consistency, persistence, representative cases, recovery |
| Text only | Operator understanding and demo consistency |

Playable old footage does not waive business revalidation.

## 7. History retention

Scan and plan-validation records are retained for review. Each belongs to its own task, version, asset, and range.

1. Identify saved task/version.
2. Open review or validation records.
3. Select by validation time.
4. Check sampling, submitted reviews, and available clips.
5. Distinguish absent samples, incomplete work, unreviewed, and reviewed evidence.
6. Preserve errors and dates; an empty directory does not mean everything passed.

Refresh and Load more read data. They do not rerun recognition or restore missing files automatically.

## 8. Low storage

1. Avoid adding more large assets; preserve draft and message.
2. Identify whether upload, recording, scan, validation, or evidence failed.
3. Have the administrator inspect space and retention needs.
4. Back up before using explicit supported cleanup controls.
5. Do not directly remove referenced media or images.
6. Reconcile the original operation before starting a duplicate.
7. Verify alarm delivery and usable recording separately.

Deleting all tasks is not routine storage maintenance.

## 9. Report a fault effectively

| Information | Include |
|---|---|
| Time/station | Actual occurrence |
| Task/plan | Names and version |
| Step | Stage and operation |
| Path | Entry button and click/drag action |
| Expected | Intended addition, replacement, save, or advancement |
| Actual | Exact visible message and state |
| Picture | Screenshot with surrounding context |
| Evidence | Asset and endpoints; whether separately recorded |
| Recovery attempted | Refresh, retry, undo, reopen |
| Impact | Production work, unknown confirmation, missing evidence |

Keep passwords out of instructions. Apply your organization's rules to account, personal, and production information before sending support material.

## 10. Station acceptance procedure

### Basic usability

Connection, playback, seeking, thumbnails, drawing, regions, scrolling, buttons, and narrow-window layout.

### Recognition

Document real behavior for targets, views, missing items, confusers, duplicates, counts, gloves, and occlusion before process logic.

### Rules

Check conditions, timing, geometry, dependencies, unordered groups, and correct confirmation identity using normal and negative cases.

### Persistence and recovery

Save/reopen consistency, bundle limits, slow network, unknown outcome, interruption, low storage, and exception recovery.

### Independent operator use

Have the actual operator complete a real workflow using only the manual and interface. Record obstacles and ambiguous words. The engineer performing it instead does not establish operator usability.

### Handover

Retain version, sources, cases, expected/actual outcomes, issues, backups, and ownership. Complete configuration screens alone do not establish acceptance.

## 11. Acceptance worksheet

| Item | Expected | Actual | Result/issue | Owner |
|---|---|---|---|---|
| Footage | Beginning/middle/end and thumbnails work |  |  |  |
| Object recognition | Correct position/count; negatives rejected |  |  |  |
| Regions | Inside/outside/boundary appropriate |  |  |  |
| Hands | Correct operator direction; no false action from occlusion |  |  |  |
| Sequence/group | Correct, reversed, omitted cases |  |  |  |
| Confirmation | Correct piece, step, acknowledgement |  |  |  |
| Persistence | Saved/reopened settings match |  |  |  |
| Fault/recovery | Configuration preserved, no false pass, traceable |  |  |  |
| Operator independence | No additional verbal guidance |  |  |  |
| Backup/handover | Files, version, responsibilities clear |  |  |  |

**Related: [Operator guide](operator-guide.md) · [Troubleshooting](troubleshooting.md) · [Button reference](control-reference.md).**

*Footage attribution: [Sample and screenshot credits](reference.md#7-sample-and-screenshot-credits).*
