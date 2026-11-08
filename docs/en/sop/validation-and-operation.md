# Validation, Saving, and Station Operation

[简体中文](../../zh/sop/validation-and-operation.md) · [Guide home](index.md)

Validate recognition, one stage, the complete plan, an independent recording, and saved station operation in that order. Establish each layer before proceeding.

## 1. Pre-validation checks

Confirm playable footage, seeking, correct targets and references, appropriate sources, regions, combination rules, timing, and completion methods. Overall and stage ranges must agree. Resolve configuration errors and unavailable targets. Verify camera orientation and operator-relative handedness when applicable.

## 2. Validate recognition and then a stage

If the original picture contains the mallet but the detector misses it, address recognition first. Changing stage order does not solve that problem.

Click the stage validation control and inspect:

1. Stage, recording, and range used.
2. Observed objects or actions.
3. The specific unmet requirement.
4. Hold time or later action still required.
5. Visual conditions met but operator review pending.
6. Missing input or ambiguous observations.

[![A stage's selected recording range](../../assets/sop/en-14-stage-range.jpg)](../../assets/sop/en-14-stage-range.jpg)

Video pixels alone cannot establish that a person performed an operator-confirmation action. Check the actual confirmation control, timing, and following stage in the interactive workflow.

## 3. Validate the plan

Use **Validate plan** to check stage order, all steps within groups, and the final state. Separate stage success does not establish correct integration if dependencies, grouping, or required steps are wrong.

| Information | Meaning | Does not establish |
|---|---|---|
| Processed frames / video position | Processing progress | The same percentage of steps passed |
| Step conditions met | Current detection requirement holds | The whole plan completed |
| Waiting for operator review | A human action is still required | The system is frozen |
| Stage passed | This stage completed using this evidence | Other stages or independent footage passed |
| Plan completed | This execution reached its final state | Acceptance for every camera or shift |
| Missing input / insufficient picture evidence | Reliable observations are unavailable | Absence, withdrawal, or successful completion |

Read checks and reasons alongside timestamps and overlays. One green box is not acceptance evidence for the complete plan.

## 4. Exercise failure paths

Include missing objects, wrong regions, too few or too many objects, a hand that never approaches or never withdraws, reversed order, exceeded deadlines, blur, occlusion, stream interruption, missing frames, and an unfinished step within an unordered group.

Expected failures should produce understandable waiting, unmet, exception, or review states rather than automatic completion. Use site quality requirements to decide whether operation may continue.

## 5. Use independent footage

Add another validation recording through the corresponding control. Record a different execution at the same station, then cover normal variation in position, speed, lighting, and operators.

Teaching footage establishes references; independent footage checks behavior beyond the original recording. Reusing the same segment is not independent validation.

Retain plan version, footage, range, and result for each run. After changing recognition or rules, check whether revalidation is required.

## 6. Three save levels

[![Outer task save and run controls](../../assets/sop/en-23-save-task.jpg)](../../assets/sop/en-23-save-task.jpg)

1. **Done** in the SOP editor returns changes to the task draft.
2. **Confirm** in node settings, if still open, returns node configuration to the outer editor.
3. **Save** in the task editor persists the task. Wait for an explicit result.

Running is separate. A browser draft is not proof that a production task uses the change.

Reopen the task and check targets, references, region geometry and assignments, sources, hands, completion, timing, order, grouping, independent stage ranges, demonstrations, instructions, and version.

Do not submit repeatedly during a slow save. Retain the window and error message, check connectivity, and follow the recovery prompt.

## 7. Station operation

After saving and starting the task, enter **Station operation** from its task card.

Read the current step and instructions. Demonstration playback should show that step's clip. Inspect unmet requirements rather than repeatedly confirming to skip them. When review is required, inspect the physical work before confirming the current step.

Follow site procedures for exceptions, stopping, and restarting a cycle. A previous cycle's completion must not be treated as completion of the current product. Changes to a running configuration require saving, checking, and revalidation.

## 8. History and storage

Scan results and plan-validation records are retained for review and traceability. Follow storage warnings and the explicit cleanup workflow. Clearing tasks, deleting videos, or re-recording is not a routine refresh mechanism.

Before cleanup, identify task, version, and retention needs, and back up according to site policy. Do not combine old-plan, current-plan, and different-recording results into one unexplained statistic.

## 9. Handover evidence

| Item | Record |
|---|---|
| Station | Camera, fixed view, lighting, valid work areas |
| Plan | Name, version, order, operator confirmation |
| Recognition | References and representative variation |
| Validation | Positive/negative cases, independent footage, ranges, results |
| Usability | An operator can follow the guide without verbal assistance |
| Maintenance | Changes requiring revalidation and exception ownership |

Configuration completeness is not production acceptance. Acceptance requires trustworthy observations and appropriate validation.

**Next: [Troubleshooting →](troubleshooting.md)**
