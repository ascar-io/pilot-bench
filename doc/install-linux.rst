Installation on Linux
*********************

We prebuild binary packages for the following Linux distributions.

Red Hat Enterprise Linux / CentOS 7
===================================

Import the Project ASCAR Nightly Repo signing key:

.. code-block:: bash

   cd /tmp
   wget https://download.ascar.io/pub/repo/RPM-GPG-KEY-ASCAR-NIGHTLY
   rpm --import RPM-GPG-KEY-ASCAR-NIGHTLY

Install the nightly repo configuration file and use yum to install
the pilot-bench package:

.. code-block:: bash

   rpm -ihv https://download.ascar.io/pub/repo/ascar-repo-el-nightly.rpm
   yum install pilot-bench

In the future you can always update to the latest nightly build by
running:

.. code-block:: bash

   yum update


Ubuntu 16.04 LTS x86-64
=======================

Native build coming soon. For now please use the generic Linux package
as shown below.


Other x86-64 Linux
==================

This generic Linux build should work on most 64-bit Linux systems:

* Latest nightly build: https://download.ascar.io/pub/repo/linux-generic-x64/nightly/pilot-bench-nightly-latest-linux-x64.tar.gz

* GPG Signature: https://download.ascar.io/pub/repo/linux-generic-x64/nightly/pilot-bench-nightly-latest-linux-x64.tar.gz.asc

Older versions can be found at: https://download.ascar.io/pub/repo/linux-generic-x64/nightly/

.. include:: signing-keys.rst
