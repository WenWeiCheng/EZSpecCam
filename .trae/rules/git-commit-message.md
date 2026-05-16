---
alwaysApply: true
scene: git_message
---

brief but complete commit message with `fix,feat,chores` prefix, for example:

```
fix: actually save frame image to disk in Save Frame functions

- on_actionSaveFrameAs_triggered(): add missing m_currentFrame.image.save() call
- on_actionSaveFrameAutoNumber_triggered(): add directory check and actual save to auto-save directory with millisecond timestamp to avoid overwrites
- Add error handling with QMessageBox on save failure
```
