Building ASCAR Pilot
####################

.. include:: warning-of-unstable.rst

.. note:: This section is intended for developers or those users who
          have a need to compile Pilot from source code. Most regular
          users just need to install our official Pilot binary, see
          :doc:`install`.

.. include:: supported-os.rst

.. include:: Requirements-and-Installation-Instructions.rest


Build Options
*************

The following are configuration options that are supported by Pilot's
CMake builder. To set an option, use: `-DOPT=VAL`. For example, on
Linux (CentOS 7) to use the system's Python 2.7 you can set
`-DWITH_PYTHON=ON -DPYTHON_LIBRARY=/usr/lib64/libpython2.7.so -DPYTHON_INCLUDE_DIR=/usr/include/python2.7`.
If you are using Python 3 from CentOS Software Collection, you can set
`-DWITH_PYTHON=ON -DPYTHON_LIBRARY=/opt/python3.5/lib/libpython3.5.so -DPYTHON_INCLUDE_DIR=/opt/python3.5/inc`.
On macOS you can set
`-DWITH_PYTHON=ON -DPYTHON_LIBRARY=/System/Library/Frameworks/Python.framework/Versions/2.7/Python -DPYTHON_INCLUDE_DIR=/System/Library/Frameworks/Python.framework/Versions/2.7/include/python2.7`.

=================== ================================================================================== ================ ==========
Option              Description                                                                        Default          Introduced
=================== ================================================================================== ================ ==========
CMAKE_BUILD_TYPE    Build type                                                                         `RelWithDebInfo` 0.1
WITH_PYTHON         Build Python binding (you may also want to set the following three options) [#f1]_ OFF              0.2
PYTHON_LIBRARY      The path of the libpython library                                                  Auto detect      0.2
PYTHON_INCLUDE_DIR  The path of pyconfig.h [#f2]_                                                      Auto detect      0.2
BOOST_PYTHON_MODULE The name of the Boost.Python module [#f3]_                                         `python`         0.2
=================== ================================================================================== ================ ==========

.. rubric:: Footnotes

.. [#f1] To enable building the Pilot Python binding, Boost.Python and
         libpython are needed and their versions have to match (e.g.,
         Boost.Python3 has to be used if you use libpython3.x). Pilot
         uses the `python` interpreter as designated by shell `PATH`,
         and tries to find the location of `libboost-python.so` and
         libpython automatically. If the detection fails or if the
         detected components have mismatched versions or if you want
         to specify the libraries to use, set the three options that
         follows in the table.

.. [#f2] Pilot uses CMake's `find_library` to search for libpython
         automatically. If you have multiple Python instances on your
         system you can set these two `PYTHON_*` options and point
         them to the correct libpython file and include path. Note
         that `PYTHON_LIBRARY` has to point to the actual dynamic
         library while `PYTHON_INCLUDE_DIR` points to a path.  You
         also need to make sure that running `python` from the command
         line invokes the Python interpreter of the same version of
         your libpython; importing a module that is linked to a
         different version of libpython would lead to crash. You can
         do so by tweaking the order of directories in `PATH` or using
         a Python virtualenv. Refer to the documentation of your shell
         and Python documentation for details.

.. [#f3] This name specifics what Boost.Python library Pilot should
         link to. The default value is `python` which means the
         library called `libboost-python.so` should be used. On Ubuntu
         Linux `libboost-python.so` is a symlink to either
         `libboost-python-py2*.so` or `libboost-python-py3*.so`. This
         library has to match the version of libpython and Python
         interpreter you want to use. For instance, you need to set it
         to `python-py35` if you want to build the Python binding for
         Python 3.5 on Ubuntu 16.04 LTS; you need to set it to
         `python3` if you are using Python 3.* and a vanilla Boost
         library.

FAQ
****

Error: undefined reference to boost::python::detail::init_module()
==================================================================

Python 2 and 3 use different ``init_module()``. If you see a link
error message like this, it is likely that your Boost library is
compiled to use a different version of Python than the libpython Pilot
is trying to link to.
