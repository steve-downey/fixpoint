# Follow-up: ill-formed overload pair in `Box::operator*()`

**File:** `src/smd/fixpoint/box.hpp`, lines 54-55 (as of commit `8ad39f4`)

```cpp
constexpr auto operator*() const -> A & { return *ptr; }
constexpr auto operator*() && -> A && { return std::move(*ptr); }
```

## Problem

These two overloads mix a non-ref-qualified member function (`const`) with a
ref-qualified one (`&&`) for the same signature. Per [over.load], within a
class it is ill-formed to declare a ref-qualified overload and a
non-ref-qualified overload of the same member function — every overload of
that name/signature must either all be ref-qualified or none of them.

This is IFNDR (ill-formed, no diagnostic required), which is presumably why
it has gone unnoticed: whatever toolchain was used to develop/test fixpoint
accepted it silently. **GCC 15.2.0 (Ubuntu) rejects it as a hard error:**

```
box.hpp:55:20: error: 'constexpr int&& Box::get() &&' cannot be overloaded
with 'constexpr int& Box::get() const'
```

(reproduced with a minimal `Box`-shaped repro, not just in-place).

## Where this was found

Discovered 2026-07-17 while vendoring a snapshot of this repo (commit
`8ad39f4`) into `generic_monads` for the monadic-algorithms plan — see that
repo's `docs/notes/s01-vendoring.org`. The vendored copy has already been
fixed there (see below); **this repo (fixpoint) itself still has the bug**,
this note is the flagged follow-up.

## Suggested fix

Add the explicit `&` lvalue ref-qualifier to the first overload:

```cpp
constexpr auto operator*() const & -> A & { return *ptr; }
constexpr auto operator*() && -> A && { return std::move(*ptr); }
```

Semantically identical (lvalue `*box` still resolves to the first overload;
rvalue `*std::move(box)` still resolves to the second), and standard-conformant.

## Scope check

Grepped the rest of `src/smd/` for the same pattern (a `&&`-qualified member
mixed with a non-ref-qualified sibling of the same signature). Only
`box.hpp` is affected — `one_shot.hpp:97`'s `operator()(Args...) &&` has no
unqualified sibling overload, so it's fine as-is.
