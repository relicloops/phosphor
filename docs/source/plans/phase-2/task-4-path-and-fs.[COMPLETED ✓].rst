.. meta::
   :title: path normalization and filesystem operations
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; path and filesystem
   single: modules; path_norm
   single: modules; fs_readwrite
   single: modules; fs_copytree
   single: modules; fs_atomic
   single: traversal prevention

task 4 -- path normalization and filesystem operations
=======================================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement path_norm.c (path normalization, traversal prevention, root escape
detection), fs_readwrite.c (byte-safe read/write), fs_copytree.c (recursive
directory copy), fs_atomic.c (atomic write: temp -> fsync -> rename),
metadata_filter.c (deny list enforcement).

depends on
----------

- phase-1/task-3-platform-layer

deliverables
------------

1. path_norm.c -- normalize paths, reject ``..`` escape, reject absolute destination (unless flagged)
2. fs_readwrite.c -- byte-slice (uint8_t* + size_t) read/write, no C string assumptions
3. fs_copytree.c -- recursive directory copy with metadata filtering
4. fs_atomic.c -- write to temp, fsync, rename; EXDEV fallback to copy+remove
5. metadata_filter.c -- deny list: .DS_Store, ._*, Thumbs.db, .Spotlight-V100, .Trashes

acceptance criteria
-------------------

- [ ] path traversal with ``..`` rejected
- [ ] absolute destination paths rejected unless explicit flag
- [ ] binary files copied byte-exact (no text transform)
- [ ] atomic write survives power-loss simulation (temp exists, rename atomic)
- [ ] EXDEV fallback works (cross-device rename -> copy + remove, logged at warn)
- [ ] metadata deny list blocks all five default patterns

references
----------

- masterplan lines 427-451 (byte precision)
- masterplan lines 677-681 (path safety)
- masterplan lines 766-779 (staging + EXDEV)
