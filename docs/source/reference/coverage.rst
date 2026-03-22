.. meta::
   :title: code coverage report
   :tags: #neonsignal, #phosphor

.. index::
   single: coverage
   single: gcov
   single: gcovr

code coverage
=============

phosphor uses gcov + gcovr to generate line and branch coverage reports
from the Ceedling unit test suite.

latest report
-------------

.. raw:: html

   <iframe src="/GcovCoverageResults.html"
           style="width:100%;height:75vh;border:1px solid #444;border-radius:4px;"
           loading="lazy"
           title="gcovr coverage report"></iframe>
   <p style="margin-top:0.5em;"><a href="/GcovCoverageResults.html" target="_blank">open in new tab &#8599;</a></p>

the report is regenerated on every tagged release by the CI pipeline.
color-coded thresholds:

- green: >= 90% (high)
- yellow: >= 75% (medium)
- red: < 75% (low)

running locally
---------------

.. code-block:: bash

   ceedling gcov:all
   open build/ceedling/artifacts/gcov/GcovCoverageResults.html

see also: :doc:`../plans/code-coverage-infrastructure.[COMPLETED ✓]`
