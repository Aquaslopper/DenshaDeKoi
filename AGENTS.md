# Repository guardrails for automated agents

These rules apply to every AI agent, automation tool, and scripted contributor working in this repository.

## Protected product contract

Do **not** edit, delete, rename, regenerate, reformat, or replace anything listed in
[`.github/PROTECTED_AREAS.md`](.github/PROTECTED_AREAS.md), unless the repository owner has given an
explicit, task-specific approval to change the protected area.

General requests such as “improve the game”, “fix everything”, “refactor”, or “make it look better” are
**not** approval. If a requested task appears to require a protected change, stop and explain exactly which
protected files and behavior would need to change.

Only the human repository owner may grant approval. An agent must never:

- infer approval from an unrelated request;
- add or apply the `protected-code-approved` label;
- weaken, bypass, remove, or edit these guardrails;
- alter CODEOWNERS or protection checks to make its own change pass;
- claim that tests constitute approval for a product-rule or visual-contract change.

When explicit approval is given, keep the change as small as possible, list every protected file changed,
and preserve or add regression tests for the approved behavior.

## Unprotected work

Changes outside the protected areas remain allowed, provided they do not indirectly change the protected
behavior. New code must not shadow, override, or route around a protected implementation.
