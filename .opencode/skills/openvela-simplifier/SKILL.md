---
name: openvela-simplifier
description: Simplify NuttX/openvela code and implementation flows by removing unnecessary complexity, redundant abstractions, patch-on-patch logic, and inconsistent patterns. Prefer existing NuttX/openvela facilities and simpler designs while preserving intended behavior. Use when reviewing, refactoring, or polishing an implementation before submission.
---

# OpenVela Simplifier

Simplify NuttX/openvela implementations while preserving intended behavior.

The primary goal is not to add more checks or abstractions, but to reduce unnecessary code and complexity.

## Core Principle

Before fixing or extending code, ask:

1. Can this code be removed?
2. Can this flow be simplified?
3. Can existing code or APIs already solve this?
4. Can duplicated or special-case logic be unified?
5. Only then consider adding new code.

Prefer the smallest implementation that correctly expresses the intended behavior.

## Simplification Rules

### 1. Remove unnecessary complexity

Look for:

- unnecessary variables, states, branches, wrappers, or helper functions
- duplicated logic
- repeated conversions or intermediate steps
- abstractions used only once without improving clarity
- overly defensive logic for cases already guaranteed by the caller or framework
- manual implementations of functionality already provided by NuttX/openvela

Prefer direct and obvious control flow.

Do not replace simple code with a more abstract design unless the abstraction clearly reduces overall complexity.

### 2. Avoid patch-on-patch logic

Do not solve structural problems by continuously adding conditions, flags, exceptions, or special cases.

When several fixes accumulate around the same logic:

- identify the original assumption or design problem
- simplify the state or control flow
- merge equivalent cases
- remove obsolete workarounds

Prefer fixing the cause instead of preserving layers of historical fixes.

### 3. Reuse existing implementations

Before introducing custom logic, check whether equivalent functionality already exists in:

- the same module
- nearby drivers or subsystems
- NuttX
- openvela
- standard system APIs

Prefer established project patterns over local reinvention.

Do not introduce a new helper, abstraction, or mechanism when an existing one provides the same behavior clearly.

### 4. Keep implementation patterns consistent

Compare similar operations and code paths.

Check for unnecessary differences in:

- function structure
- variable and constant naming
- initialization and cleanup
- error handling
- state transitions
- configuration handling
- resource management

Equivalent operations should normally follow the same pattern unless there is a real behavioral difference.

Prefer the style already used by surrounding NuttX/openvela code.

### 5. Follow NuttX/openvela conventions

Ensure simplification does not violate project conventions.

Follow:

- repository `AGENTS.md` instructions when present
- existing subsystem conventions
- NuttX/openvela naming and formatting rules
- `.clang-format`
- `nxstyle`
- established error-handling and configuration patterns

Do not invent a new local convention when the project already has one.

### 6. Simplify comments

Comments should explain information that is not obvious from the code.

Prefer comments describing:

- intent
- constraints
- hardware or protocol requirements
- non-obvious design decisions

Avoid comments that:

- restate the code
- enumerate every normal case
- describe implementation details that can be expressed clearly by the code itself
- preserve explanations for logic that can simply be removed or simplified

Keep comments general when the same explanation covers multiple cases.

Document individual cases only when they are genuinely exceptional.

## Simplification Process

Review the implementation in this order:

1. **Delete**
   - Remove unnecessary code, states, checks, comments, and abstractions.

2. **Merge**
   - Combine duplicated or equivalent paths.

3. **Reuse**
   - Replace custom implementations with existing NuttX/openvela facilities when appropriate.

4. **Restructure**
   - Simplify control flow when accumulated conditions indicate a deeper design problem.

5. **Unify**
   - Make naming and implementation patterns consistent with surrounding code.

6. **Polish**
   - Simplify comments and apply NuttX/openvela style conventions.

Do not start by adding new code.

## Constraints

Preserve intended behavior unless a behavioral change is explicitly requested.

Do not simplify code based on assumptions about hardware, protocol behavior, concurrency, timing, or external requirements.

If an apparently unnecessary piece of logic may represent an undocumented constraint, investigate surrounding code or ask for clarification instead of removing it blindly.

Do not trade correctness or readability for fewer lines of code.

"Simpler" means fewer concepts and special cases, not merely shorter source code.

## Output

When reporting simplification opportunities, focus on meaningful changes.

For each issue, briefly state:

- what is unnecessarily complex
- why it can be simplified
- the simpler approach

Prioritize structural simplifications over minor formatting issues.

If the implementation is already appropriately simple, say so instead of inventing changes.
