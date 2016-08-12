.. ASCAR Pilot documentation master file, created by
   sphinx-quickstart on Mon Jul 11 19:13:04 2016.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to ASCAR Pilot's documentation!
=======================================

About ASCAR Pilot
`````````````````

The Pilot Benchmark Framework provides a tool (bench) and a library
(libpilot) to automate computer performance measurement tasks. This is
a project under active development. The project is designed to answer
questions like these that rise often from performance measurement:

*  How long shall I run this benchmark to get a precise result?
*  Is this new scheduler really 3% faster than our old one or is that a
   measurement error?
*  Which setting is faster for my database, 20 worker threads or 25 threads?

.. note::

   Both Pilot and this documentation are in alpha stage and are
   under active development. There may be inconsistencies, missing
   parts, or even errors in this documentation. Should you notice
   anything wrong, please drop us a message. We also welcome
   contributions! Please fork the code (which includes this
   documentation) on github and hack. Send us a pull request when you
   make something interesting. Thank you!


The main documentation for the site is organized into the following sections:

*  :ref:`user-docs`
*  :ref:`dev-docs`

.. _user-docs:

.. toctree::
   :maxdepth: 2
   :caption: User Documentation

   what-is-pilot
   install
   tutorial-list
   features
..   settings
..   support
..   known-issues
..   faq


.. _dev-docs:

.. toctree::
   :maxdepth: 2
   :caption: Developer Documentation

   build
..   changelog
..   contribute
..   tests
..   architecture
..   development/standards
..   development/buildenvironments


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

Part of this documentation has been published as paper: Yan Li, Yash
Gupta, Ethan L. Miller, and Darrell D. E. Long. Pilot: A Framework
that Understands How to Do Performance Benchmarks the Right Way. In
*Proceedings of IEEE 24th International Symposium on Modeling,
Analysis, and Simulation of Computer and Telecommunication Systems
(MASCOTS'16)*, 2016.

This is a research project from the `Storage Systems Research Center
<http://www.ssrc.ucsc.edu/>`_ in `UC Santa Cruz <http://ucsc.edu>`_.
This research was supported in part by the National Science Foundation
under awards IIP-1266400, CCF-1219163, CNS-1018928, CNS-1528179, by
the Department of Energy under award DE-FC02-10ER26017/DESC0005417, by
a Symantec Graduate Fellowship, by a grant from Intel Corporation, and
by industrial members of the `Center for Research in Storage Systems
<http://www.crss.ucsc.edu/>`_.  Any opinions, findings, and
conclusions or recommendations expressed in this material are those of
the author(s) and do not necessarily reflect the views of the sponsors
listed above.
