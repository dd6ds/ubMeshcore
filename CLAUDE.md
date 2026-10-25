# Instructions for coding assistants

This app is published on the OpenStore, where people review the source code.
Code that reads as machine-generated (stubs, dead code, helper notes,
oversized files) has been rejected there before.

**Read and follow [CONTRIBUTING.md](CONTRIBUTING.md) for every change.** Its
checklist (section 9) must be satisfied before a change counts as done.

In particular:

- Change only what the task needs, in the style of the surrounding code.
- Do not create notes, summaries, to-do lists, fix scripts or "changes"
  files in the repository. Report to the user in the conversation instead.
- Delete code that your change makes unused.
- Keep files and functions small (CONTRIBUTING.md, section 3); split instead
  of growing them.
- Build with `clickable build` to check a change. See the README for
  translations (`lupdate` + `i18n/translate_de.py`) and Qt 5.12 limits.
