.. meta::
   :title: package manager distribution
   :tags: #neonsignal, #phosphor
   :status: deferred
   :updated: 2026-02-14

.. index::
   triple: phase 6; task; packaging
   single: homebrew
   single: deb
   single: rpm

task 2 -- package manager distribution
========================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

create distribution packages for system installation -- homebrew tap formula
for macos, deb/rpm artifacts for linux. includes meson install target.

depends on
----------

- phase-6/task-1-ci-matrix

deliverables
------------

1. ``meson install -C build`` target to system path
2. homebrew tap formula (macos)
3. deb package configuration (debian/ubuntu)
4. rpm spec (fedora/rhel) -- later priority
5. install documentation

acceptance criteria
-------------------

- [ ] ``meson install -C build`` places binary in /usr/local/bin (or configured prefix)
- [ ] homebrew formula installs and runs correctly
- [ ] ``brew install phosphor`` works from tap
- [ ] deb package installs on ubuntu/debian

references
----------

- masterplan lines 967-971 (distribution options)
