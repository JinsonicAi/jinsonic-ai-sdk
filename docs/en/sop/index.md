# SOP System User Guide

[简体中文](../../zh/sop/index.md)

This guide is for process engineers, operators, deployment teams, and administrators using AIBox SOP for the first time. A fixed-camera workbench example takes you through **choosing a camera → preparing a video → selecting a target → improving recognition → defining steps → validating stages → validating the plan → saving → station operation**. You do not need to write code, rule expressions, or JSON.

**For your first plan, follow the chapters in order.** If you already have a plan, use the table below.

| What do you need to do? | Chapter |
|---|---|
| Build your first understandable plan | [Quick start: tool readiness and operator confirmation](quickstart.md) |
| Record, upload, play, or select a video range | [Video, targets, and recognition](video-and-recognition.md) |
| Draw target boxes, add angles, review misses and duplicate boxes | [Video, targets, and recognition](video-and-recognition.md) |
| Check only selected areas, counts, or object positions | [Rules, regions, and timing](rules-and-regions.md) |
| Configure either hand, left/right hands, approach, withdrawal, or a transfer | [Hand operations](hand-operations.md) |
| Drag objects, hands, and regions; understand highlighting, replacement, and undo | [Toolbar and drag-and-drop](toolbar-and-drag.md) |
| Validate, confirm, save, operate, and review history | [Validation, saving, and station operation](validation-and-operation.md) |
| Resolve missing pictures, unavailable buttons, or uncertain results | [Troubleshooting and maintenance](troubleshooting.md) |
| Check a plan before handover or look up a term | [Checklists and glossary](reference.md) |

!!! note "Applicable interface and illustrations"
    This guide is based on device Web interfaces captured in November 2026. The device application, algorithm plugins, device Web interface, and cloud Web interface must be compatible. Optional features depend on the controls available on your device. Addresses, names, times, and reference counts in the illustrations are examples.

    This English guide uses separate captures of the English interface. Names and instructions entered by users are project data: changing the interface language does not automatically translate them. In the example, 橡胶锤 means “rubber mallet,” 工具工作区 means “tool work area,” and 人工确认完成 means “operator confirmation.”

Click an illustration to view the original image. A screenshot showing “Awaiting detection” or “Not validated” illustrates that state; it does not certify a successful acceptance test.

## 1. Understand what the system checks

SOP converts a process requirement into observable conditions:

- **Objects:** whether a mallet is visible, whether the count is correct, and whether it is in the tool work area.
- **Spatial relationships:** whether object A is inside, to the left or right of, or near object B; whether a visible hand is near or away from an object.
- **Timing:** how long a condition must remain satisfied and how much time a step may take.
- **Order:** which step comes first and which steps may be completed in either order.
- **Operator confirmation:** which requirements need a person to inspect and confirm.

The system uses camera observations and configured rules to guide and check a process. A hand being near a tool does not by itself prove a secure grasp, a correctly tightened screw, or a properly assembled part. Retain operator review for requirements that the image cannot reliably establish.

## 2. The example used throughout this guide

The example video shows a fixed-camera workbench with a rubber mallet, drill, paint cup, and other tools. The introductory workflow uses only the mallet, so you can learn one condition before combining several.

| Stage | Step | Requirement | Completion | Example range |
|---|---|---|---|---|
| 1 | Mallet ready | The mallet is clearly visible; optionally restrict it to the tool work area | Automatically after 0.8 seconds of satisfaction | Seconds 10–16 of the source video |
| 2 | Operator confirmation | The operator checks the tools and station | Operator confirms | Seconds 16–20 of the source video |

The full teaching video is approximately 60 seconds. These times explain range selection; choose times that match your own recording. After the two-step example, extend the plan to several tools, regions, hand operations, and unordered steps.

[![Figure 1: SOP editor for the workbench example](../../assets/sop/en-04-editor-overview.jpg)](../../assets/sop/en-04-editor-overview.jpg)

*Figure 1: Select a step on the left, inspect video and targets in the center, and manage references on the right. Plan and camera controls are above; authoring and validation continue below.*

## 3. Terms that should remain distinct

| Term | Purpose | Does not mean |
|---|---|---|
| Teaching video | Shows the standard operation; used to select targets and clips and for operator playback | A video whose recognition accuracy has already been accepted |
| Target reference image | Describes the appearance of one target; several angles may belong to that target | The number of physical objects required in the scene |
| Manual box | Marks an object that should be present in this particular frame | An independently produced algorithm detection |
| Target frame scan | Samples the selected video range to inspect recognition | Every video frame has been manually reviewed |
| Stage validation range | Tests one stage using a selected recording segment | The complete plan has passed |
| Demonstration clip | Shows an operator how to perform one step | A command to change every other stage’s range |
| Plan validation range | Defines the recording range used to test the authored plan | The duration of a live production run |
| Save task | Applies the edited task configuration to the device | Automatically starts production |
| Station operation | Shows operators the current task, steps, and confirmation controls | The sampled-frame recognition review page |

A **stage** is an execution group. It can contain one step or an unordered group. Stages execute from top to bottom. Steps in an unordered group may finish in either order, and all must finish before the next stage. This differs from several conditions being true simultaneously; see [Rules, regions, and timing](rules-and-regions.md).

## 4. Recommended learning path

1. Build “mallet ready → operator confirmation” using the [quick start](quickstart.md).
2. Inspect recognition at different positions and angles and with hand occlusion using the [recognition chapter](video-and-recognition.md).
3. Add the regions, counts, timing, and grouping your process requires.
4. If hands matter, complete [hand-direction calibration and hand-operation setup](hand-operations.md).
5. Validate individual stages and then the plan. Review results before saving and entering [station operation](validation-and-operation.md).
6. Use the [handover checklist](reference.md) for normal operation, missing parts, wrong order, occlusion, stream loss, and recovery.

Avoid changing references, thresholds, regions, timing, and order together. Resolve one diagnosed issue, then recheck its effect so that you can tell which change helped.

## 5. Responsibilities

| Role | Main responsibility |
|---|---|
| Process/configuration personnel | Prepare teaching footage, name targets and steps, define rules, inspect recognition, validate, and save |
| Station operators | Follow the station display, review requirements needing confirmation, and respond to guidance |
| Administrators/deployment personnel | Maintain cameras, detection sources, compatible versions, storage, networking, and runtime placement |

**Next: [Build the first two-step example →](quickstart.md)**
