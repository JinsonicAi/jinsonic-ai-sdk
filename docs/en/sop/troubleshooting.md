# Troubleshooting and Recovery

[简体中文](../../zh/sop/troubleshooting.md) · [Guide home](index.md)

Retain the current configuration. Investigate connection/video, recognition, regions/conditions, stages/plan, and saving/runtime in that order. Deleting tasks or repeatedly recording again is not a recovery procedure.

## 1. Video and timeline

| Symptom | Check and action |
|---|---|
| Black or unplayable video | Check connection, processing status, and camera availability; retain the error |
| Reference image visible but no timeline pictures | References and video thumbnails are separate; verify playback and wait for the timeline |
| Dragging does not preview | Check for valid video and finish drawing or another active edit mode |
| Start and end unexpectedly close | Edit the endpoint times directly and check units and duration |
| Disabled buttons | Read the message about video, source, capability, or configuration readiness |
| Old clips changed after recording again | Check whether complete-process replacement was confirmed |

[![Loaded video and timeline](../../assets/sop/en-06-teaching-video.jpg)](../../assets/sop/en-06-teaching-video.jpg)

### Stage selection and demonstration differ

Check whether you edited validation or demonstration mode, whether the step uses separate material, and the actual player endpoints. Close and reopen the player and check persistence.

If inconsistent, retain the stage name, both ranges, and screenshots. Do not change other stages merely to make displays agree. Stage ranges are independent and may overlap.

## 2. Manual boxes and recognition

**Capacity did not increase:** A saved annotation is not necessarily adopted. Check pending state, adoption result, duplicate references, and capacity.

**Annotated object still missed:** The manual box describes the expected location, not algorithm output. Check matched annotations and positions, object scale, background, reference quality, score gates, and filtering.

**High score without completion:** Check the correct object and location, region, count, step-specific score requirement, hold time, other mandatory conditions, and pending operator review.

**Two boxes on one object:** Investigate duplicate detections and references. Do not treat two boxes as two physical objects or increase the required count to conceal the duplication.

**Cannot start a new box inside an old box:** Check new-box versus select/move mode. Cancel the old selection and use the explicit addition entry. If still blocked, retain correct annotations and report the start position and visible hint rather than deleting a valid box.

## 3. Regions and rules

| Problem | Check |
|---|---|
| Outside objects affect the step despite a drawn region | The requirement's actual region assignment |
| An inside object does not satisfy the step | Detection box, score, count, boundary, and selected region |
| Several objects appear but the combination fails | Simultaneous observations, occlusion, combination mode, and timing |
| Grouped steps run in a different order | An unordered group allows this; use sequential stages for dependencies |
| Later condition already true but sequence incomplete | A valid change after the previous phase may be required |
| Read-only or unavailable capability | Retain configuration and inspect capability; disabled controls do not establish activation |

## 4. Hands and spatial actions

**Reversed hand labels:** Verify the operator's own hands, camera mirroring, and orientation. Adjust handedness in detection settings and validate actual pictures. Renaming a step does not calibrate hands.

**An undetected hand is not considered far away:** Missing observation can result from occlusion, blur, leaving the view, or a detection miss. Far-away requires a reliable visible relationship. Improve the view or use appropriate operator review.

**Approach succeeded but step did not complete:** Approach is only one condition. Withdrawal, arrival at a destination, other phases, and completion method may remain.

**Gloves or several people:** Validate actual gloves, pose, light, and speed. A hand-count limit does not prove two hands belong to one person. Adjust the viewpoint or workflow if identity cannot be established reliably.

## 5. Dragging, scrolling, and editing

A highlighted drop without an added condition should be checked using the equivalent click entry, read-only state, duplicates, and capacity. Read whether a drop replaces or adds, and undo an unintended replacement.

For scrolling over video, release zoom modifiers and exit a drag/pan state. Ordinary scrolling browses the page; plus/minus controls can zoom the picture. Scroll long reference lists within their panel.

On narrow screens, collapse the step list and maximize the editor. After text editing, check which step is current before proceeding.

## 6. Validation and saving

**Processing reached 100%:** This is processing progress, not a pass rate. Inspect unmet steps, confirmation requirements, and input problems.

**Can an old result validate a change?** Check version, recording, range, sources, and parameters. A result marked for revalidation cannot establish that the current change passed.

**Done did not update the device:** Check node confirmation and outer task saving. Done, Save, and Run are separate. Reopen the saved configuration before starting.

**Connection lost during save:** Retain the window and error, wait for recovery, and inspect submission state. Do not repeatedly save or recreate a same-name task before establishing what persisted.

**Low storage:** Avoid unnecessary repeated scans, identify retained records, back up according to site policy, and clean an explicit scope rather than all customer data.

## 7. Information for support

Provide device/software/browser versions, task and plan names, plan version, exact entry and action, recording duration and selected range, problem timestamp, expected versus actual result, screenshots, minimal reproduction, save/run state, and recent camera or source changes.

Do not include passwords or full camera credentials in screenshots or reports.
