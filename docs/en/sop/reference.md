# Controls, Glossary, and Acceptance Checklist

[简体中文](../../zh/sop/reference.md) · [Guide home](index.md)

Use this page as a quick reference. For a first setup, follow the [introductory workflow](quickstart.md).

## 1. Control reference

| Control | Purpose | Check afterward |
|---|---|---|
| Add Task → SOP workflow | Create a process task | Correct object or hand mode |
| Choose / change | Select station camera | Correct source and station |
| Upload / Record | Provide teaching footage | Playback and thumbnails ready |
| Add target | Create a logical object and reference | Correct box and name |
| Add view | Add an appearance of the same target | No unintended separate object |
| Improve recognition | Sample and review recognition | Range, misses, false detections |
| Detect current image again | Test the current frame | Algorithm boxes match annotations |
| Adopt annotations | Add references to the plan | Pending count, references, capacity |
| Draw region | Define a reusable work area | Correct shape and position |
| Step checks → Edit | Configure objects and actions | Endpoints, regions, combination |
| Completion → Settings | Choose automatic/manual completion | Hold time and review timing |
| Stage validation range | Set that stage's clip | Endpoints and demonstration |
| Validate stage / Validate plan | Review the selected level | Reasons and final state |
| Detection settings | Shared sources and handedness | Affected steps |
| Reorder | Change stage and group relationships | Dependencies and unordered groups |
| Undo | Revert the latest supported change | Actual requirements restored |
| Done | Return changes to task draft | Continue confirmation and saving |
| Save | Persist the task | Success and reopening |
| Run / Station operation | Start saved configuration and operator view | Current version and step |

## 2. Glossary

| Term | Meaning |
|---|---|
| Target | A kind of object to recognize, such as a mallet |
| Reference image | An example of its appearance |
| Reference view | Another normal appearance of the same target |
| Manual annotation | A person's expected object location in a frame |
| Candidate | A possible detection requiring interpretation |
| Confidence / score | A recognition value, not plan accuracy |
| Threshold | A gate for accepting a candidate |
| Global region | A work-area definition reusable by steps |
| Condition | One requirement within a step |
| All simultaneously | All requirements hold at the same time |
| Any one | At least one genuine alternative holds |
| In order | Conditions occur in the configured order |
| Unordered group | All grouped steps are required, in any order |
| Hold time | How long a condition must remain satisfied |
| Deadline | Maximum permitted time for completion |
| Operator review | Visual conditions first, human inspection afterward |
| Operator confirmation | Human confirmation provides completion evidence |
| Teaching recording | Footage used to establish references and process |
| Independent validation footage | Another recording checking behavior beyond the original |
| Draft | Editable configuration, not necessarily saved or running |

## 3. Choose a hand workflow

| Requirement | Starting configuration | Validation focus |
|---|---|---|
| Tool readiness only | Object detection | Misses, duplicates, area, count |
| Hand visible | Hand detection | Occlusion, handedness, multiple people |
| Hand near object | Hand/object proximity | Both visible and reliable |
| Approach then withdraw | Ordered template | Real withdrawal after approach, not lost detection |
| Move between positions | Supported transfer template | Origin, process, destination, physical instance |
| Quality cannot be judged visually | Visual review or manual confirmation | Clear instructions and correct confirmation |

## 4. Interface language and user data

The interface provides Chinese, English, Vietnamese, Khmer, and Lao language entries. Task, plan, target, and region names and user-written instructions are not automatically translated. Use names understood by the site team.

The Chinese and English guides use their corresponding interface language. Updates may move controls; follow their purpose and current hints. Report fixed interface text that does not match the selected language rather than changing business data to imitate a button translation.

## 5. Pre-operation checklist

### Station and media

- [ ] Fixed camera, appropriate orientation, lighting, and picture quality.
- [ ] Playable, seekable recording and correct demonstration clips.
- [ ] Regions match the physical workplace.
- [ ] Ownership of revalidation after camera/media changes is clear.

### Recognition and requirements

- [ ] Representative references for every target.
- [ ] Misses, false detections, and duplicates reviewed in normal clear views.
- [ ] Quantity, regions, and handedness checked.
- [ ] Spatial relationships are not treated as proof of grasp, contact, or quality.
- [ ] Simultaneous, alternative, ordered, and grouped requirements match the process.
- [ ] Hold times, deadlines, and human confirmation are appropriate.

### Validation and operation

- [ ] Target, stage, and complete plan reviewed progressively.
- [ ] Wrong-action cases and independent footage included.
- [ ] Missing input, occlusion, and stream loss cannot masquerade as completion.
- [ ] Results match the current version and range.
- [ ] Reopened configuration checked after Done, Confirm, and Save.
- [ ] Operator can complete the process using instructions and demonstrations.
- [ ] History, storage cleanup, and exception ownership are defined.

## 6. Learning path

[Quick start](quickstart.md) → [Video and recognition](video-and-recognition.md) → [Regions and checks](rules-and-regions.md) → [Hand operations](hand-operations.md) → [Toolbar and dragging](toolbar-and-drag.md) → [Validation and operation](validation-and-operation.md).

Use [Troubleshooting](troubleshooting.md) without rebuilding the task from scratch.
